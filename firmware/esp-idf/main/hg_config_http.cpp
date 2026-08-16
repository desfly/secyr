#include "hg_config_http.hpp"

#include "hg_config_backup.hpp"
#include "hg_config_transaction.hpp"
#include "cJSON.h"

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstddef>
#include <string>

namespace homeguard::idf {
namespace {

constexpr std::size_t kMaxExportRequest = 512U;
constexpr std::size_t kMaxImportRequest = 12288U;

ConfigHttp* self_from(httpd_req_t* request) {
    return request == nullptr ? nullptr : static_cast<ConfigHttp*>(request->user_ctx);
}

bool receive_body(httpd_req_t* request, std::size_t maximum, std::string& body) {
    if (request == nullptr || request->content_len == 0U || request->content_len > maximum) return false;
    body.assign(request->content_len, '\0');
    std::size_t received_total = 0U;
    while (received_total < body.size()) {
        const int received = httpd_req_recv(
            request,
            body.data() + received_total,
            body.size() - received_total);
        if (received <= 0) return false;
        received_total += static_cast<std::size_t>(received);
    }
    return true;
}

bool json_string(const cJSON* object, const char* key, std::string& value) {
    const auto* item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!cJSON_IsString(item) || item->valuestring == nullptr) return false;
    value = item->valuestring;
    return true;
}

esp_err_t send_json(httpd_req_t* request, const std::string& body) {
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

esp_err_t reject(httpd_req_t* request, const char* status, const char* reason) {
    httpd_resp_set_status(request, status);
    return send_json(request, std::string{"{\"ok\":false,\"reason\":\""} + reason + "\"}");
}

void delayed_config_reboot(void*) {
    vTaskDelay(pdMS_TO_TICKS(350));
    esp_restart();
}

}  // namespace

esp_err_t ConfigHttp::register_handlers(
    httpd_handle_t server,
    homeguard::AccessControl* access,
    AccessNvsStore* access_store,
    NetworkHttp* network,
    CloudNvsStore* cloud_store,
    CommissioningNvsStore* commissioning_store) {
    if (server == nullptr || access == nullptr || access_store == nullptr || network == nullptr ||
        cloud_store == nullptr || commissioning_store == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    access_ = access;
    access_store_ = access_store;
    network_ = network;
    cloud_store_ = cloud_store;
    commissioning_store_ = commissioning_store;

    const httpd_uri_t export_route{
        .uri = "/api/v1/config/export",
        .method = HTTP_POST,
        .handler = &ConfigHttp::export_post,
        .user_ctx = this,
    };
    const httpd_uri_t import_route{
        .uri = "/api/v1/config/import",
        .method = HTTP_POST,
        .handler = &ConfigHttp::import_post,
        .user_ctx = this,
    };

    auto error = httpd_register_uri_handler(server, &export_route);
    if (error != ESP_OK) return error;
    return httpd_register_uri_handler(server, &import_route);
}

esp_err_t ConfigHttp::export_post(httpd_req_t* request) {
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_export(request);
}

esp_err_t ConfigHttp::import_post(httpd_req_t* request) {
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : self->handle_import(request);
}

esp_err_t ConfigHttp::handle_export(httpd_req_t* request) {
    std::string body;
    if (!receive_body(request, kMaxExportRequest, body)) return reject(request, "400 Bad Request", "invalid_body");

    cJSON* root = cJSON_ParseWithLength(body.data(), body.size());
    if (root == nullptr || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return reject(request, "400 Bad Request", "invalid_json");
    }

    std::string actor;
    std::string credential;
    std::string confirm;
    const bool fields_ok = json_string(root, "actor", actor) &&
        json_string(root, "credential", credential) && json_string(root, "confirm", confirm);
    cJSON_Delete(root);
    if (!fields_ok) return reject(request, "401 Unauthorized", "credential_required");
    if (confirm != "INCLUDE_SECRETS") return reject(request, "409 Conflict", "explicit_secret_confirmation_required");

    const auto decision = access_->authorize(actor, credential, "config.export");
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return send_json(request, std::string{"{\"ok\":false,\"reason\":\""} + homeguard::to_string(decision) + "\"}");
    }

    ConfigBackupV1 backup{};
    backup.secrets_included = true;
    backup.access = *access_;

    auto error = network_->snapshot_persisted_credentials(
        backup.wifi_ssid, backup.wifi_password, backup.wifi_present);
    if (error != ESP_OK) return reject(request, "500 Internal Server Error", "wifi_snapshot_failed");

    error = cloud_store_->load(backup.cloud);
    if (error == ESP_OK) backup.cloud_present = true;
    else if (error == ESP_ERR_NVS_NOT_FOUND) backup.cloud_present = false;
    else return reject(request, "500 Internal Server Error", "cloud_snapshot_failed");

    error = commissioning_store_->load_commissioning(backup.commissioning);
    if (error == ESP_OK) backup.commissioning_present = true;
    else if (error == ESP_ERR_NVS_NOT_FOUND) backup.commissioning_present = false;
    else return reject(request, "500 Internal Server Error", "commissioning_snapshot_failed");

    const std::string encoded = ConfigBackupV1Codec::encode(backup);
    if (encoded.empty()) return reject(request, "500 Internal Server Error", "encode_failed");

    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    httpd_resp_set_hdr(request, "Content-Disposition", "attachment; filename=homeguard-config-v1.json");
    return httpd_resp_send(request, encoded.c_str(), static_cast<ssize_t>(encoded.size()));
}

esp_err_t ConfigHttp::handle_import(httpd_req_t* request) {
    std::string body;
    if (!receive_body(request, kMaxImportRequest, body)) return reject(request, "400 Bad Request", "invalid_body");

    cJSON* root = cJSON_ParseWithLength(body.data(), body.size());
    if (root == nullptr || !cJSON_IsObject(root)) {
        cJSON_Delete(root);
        return reject(request, "400 Bad Request", "invalid_json");
    }

    std::string actor;
    std::string credential;
    std::string confirm;
    if (!json_string(root, "actor", actor) || !json_string(root, "credential", credential) ||
        !json_string(root, "confirm", confirm)) {
        cJSON_Delete(root);
        return reject(request, "401 Unauthorized", "credential_required");
    }
    if (confirm != "APPLY_CONFIG") {
        cJSON_Delete(root);
        return reject(request, "409 Conflict", "explicit_confirmation_required");
    }

    const auto decision = access_->authorize(actor, credential, "config.import");
    if (decision != homeguard::AuditDecision::Allowed) {
        cJSON_Delete(root);
        httpd_resp_set_status(request, "403 Forbidden");
        return send_json(request, std::string{"{\"ok\":false,\"reason\":\""} + homeguard::to_string(decision) + "\"}");
    }

    const cJSON* backup_object = cJSON_GetObjectItemCaseSensitive(root, "backup");
    if (!cJSON_IsObject(backup_object)) {
        cJSON_Delete(root);
        return reject(request, "400 Bad Request", "missing_backup");
    }

    char* printed = cJSON_PrintUnformatted(backup_object);
    cJSON_Delete(root);
    if (printed == nullptr) return reject(request, "500 Internal Server Error", "backup_encode_failed");
    std::string backup_json{printed};
    cJSON_free(printed);

    ConfigBackupV1 backup{};
    std::string reason;
    if (!ConfigBackupV1Codec::decode(backup_json, backup, reason)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return send_json(request, std::string{"{\"ok\":false,\"reason\":\""} + reason + "\"}");
    }
    if (!backup.secrets_included) return reject(request, "409 Conflict", "restorable_secrets_required");

    const ConfigTransaction transaction{*access_store_, *network_, *cloud_store_, *commissioning_store_};
    if (!transaction.apply(backup, reason)) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return send_json(request, std::string{"{\"ok\":false,\"reason\":\""} + reason + "\"}");
    }

    // The transaction is already committed. Schedule reboot before replying so
    // a disappearing socket cannot leave live runtime state inconsistent with NVS.
    const auto reboot_task = xTaskCreate(
        &delayed_config_reboot,
        "hg_cfg_reboot",
        2048,
        nullptr,
        5,
        nullptr);
    if (reboot_task != pdPASS) {
        esp_restart();
        return ESP_FAIL;
    }

    static constexpr char response[] =
        "{\"ok\":true,\"state\":\"config_imported\",\"rebooting\":true}";
    return httpd_resp_send(request, response, sizeof(response) - 1U);
}

}  // namespace homeguard::idf
