#include "provisioning_https.hpp"
#include "cJSON.h"
#include "esp_https_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace {
constexpr char tag[] = "hg_provision_https";
constexpr size_t max_body = 2048;
int send_json(httpd_req_t* request, const char* status, const std::string& body) {
    httpd_resp_set_status(request, status);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}
std::string json_string(cJSON* root, const char* key) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    return cJSON_IsString(item) && item->valuestring ? item->valuestring : "";
}
bool read_json(httpd_req_t* request, std::array<char, max_body + 1>& body, cJSON*& root) {
    if (request->content_len <= 0 || static_cast<size_t>(request->content_len) > max_body) return false;
    size_t offset = 0;
    while (offset < static_cast<size_t>(request->content_len)) {
        const int read = httpd_req_recv(request, body.data() + offset, request->content_len - static_cast<int>(offset));
        if (read <= 0) return false;
        offset += static_cast<size_t>(read);
    }
    body[offset] = '\0';
    root = cJSON_ParseWithLength(body.data(), offset);
    return root != nullptr;
}
const char* code_name(hg::ProvisioningCode code) {
    switch (code) {
        case hg::ProvisioningCode::Accepted: return "accepted";
        case hg::ProvisioningCode::Expired: return "expired";
        case hg::ProvisioningCode::InvalidProof: return "invalid_proof";
        case hg::ProvisioningCode::LockedOut: return "locked_out";
        case hg::ProvisioningCode::InvalidPayload: return "invalid_payload";
        case hg::ProvisioningCode::StorageFailure: return "storage_failure";
        case hg::ProvisioningCode::AlreadyProvisioned: return "already_provisioned";
        default: return "invalid_state";
    }
}
uint64_t now_ms() { return static_cast<uint64_t>(esp_timer_get_time() / 1000); }
}

bool ProvisioningHttpsServer::begin(ProvisioningService& service, const FactoryProvisioningIdentity& identity, uint16_t port) {
    if (server_ || !identity.valid() || port == 0U) return false;
    service_ = &service;
    identity_ = &identity;
    httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();
    config.httpd.server_port = port;
    config.servercert = reinterpret_cast<const uint8_t*>(identity.certificate_pem.c_str());
    config.servercert_len = identity.certificate_pem.size() + 1U;
    config.prvtkey_pem = reinterpret_cast<const uint8_t*>(identity.private_key_pem.c_str());
    config.prvtkey_len = identity.private_key_pem.size() + 1U;
    httpd_handle_t handle = nullptr;
    if (httpd_ssl_start(&handle, &config) != ESP_OK) return false;
    server_ = handle;

    const httpd_uri_t info_uri{"/v1/provisioning/info", HTTP_GET, &ProvisioningHttpsServer::info_entry, this};
    const httpd_uri_t authorize_uri{"/v1/provisioning/authorize", HTTP_POST, &ProvisioningHttpsServer::authorize_entry, this};
    const httpd_uri_t apply_uri{"/v1/provisioning/apply", HTTP_POST, &ProvisioningHttpsServer::apply_entry, this};
    if (httpd_register_uri_handler(static_cast<httpd_handle_t>(server_), &info_uri) != ESP_OK ||
        httpd_register_uri_handler(static_cast<httpd_handle_t>(server_), &authorize_uri) != ESP_OK ||
        httpd_register_uri_handler(static_cast<httpd_handle_t>(server_), &apply_uri) != ESP_OK) {
        stop();
        return false;
    }
    ESP_LOGI(tag, "provisioning HTTPS server started");
    return true;
}

void ProvisioningHttpsServer::stop() {
    if (server_) httpd_ssl_stop(static_cast<httpd_handle_t>(server_));
    server_ = nullptr; service_ = nullptr; identity_ = nullptr;
}

int ProvisioningHttpsServer::info_entry(httpd_req_t* request) { return static_cast<ProvisioningHttpsServer*>(request->user_ctx)->info(request); }
int ProvisioningHttpsServer::authorize_entry(httpd_req_t* request) { return static_cast<ProvisioningHttpsServer*>(request->user_ctx)->authorize(request); }
int ProvisioningHttpsServer::apply_entry(httpd_req_t* request) { return static_cast<ProvisioningHttpsServer*>(request->user_ctx)->apply(request); }

int ProvisioningHttpsServer::info(httpd_req_t* request) {
    if (!service_) return send_json(request, "503 Service Unavailable", R"({"ok":false})");
    const auto state = static_cast<unsigned>(service_->status(now_ms()).state);
    return send_json(request, "200 OK", "{\"ok\":true,\"device_id\":\"" + service_->device_id() + "\",\"state\":" + std::to_string(state) + "}");
}

int ProvisioningHttpsServer::authorize(httpd_req_t* request) {
    std::array<char, max_body + 1> body{}; cJSON* root = nullptr;
    if (!service_ || !read_json(request, body, root)) return send_json(request, "400 Bad Request", R"({"ok":false,"error":"invalid_json"})");
    const auto code = service_->authorize(json_string(root, "pairing_code"), json_string(root, "certificate_sha256"), now_ms());
    cJSON_Delete(root);
    std::fill(body.begin(), body.end(), '\0');
    const bool ok = code == hg::ProvisioningCode::Accepted;
    return send_json(request, ok ? "200 OK" : "403 Forbidden", std::string("{\"ok\":") + (ok ? "true" : "false") + ",\"result\":\"" + code_name(code) + "\"}");
}

int ProvisioningHttpsServer::apply(httpd_req_t* request) {
    std::array<char, max_body + 1> body{}; cJSON* root = nullptr;
    if (!service_ || !read_json(request, body, root)) return send_json(request, "400 Bad Request", R"({"ok":false,"error":"invalid_json"})");
    hg::ProvisioningPayload payload{
        json_string(root, "wifi_ssid"), json_string(root, "wifi_password"),
        json_string(root, "cloud_endpoint"), json_string(root, "cloud_token"),
        json_string(root, "local_api_token"), json_string(root, "owner_label")
    };
    cJSON_Delete(root);
    std::fill(body.begin(), body.end(), '\0');
    const auto code = service_->apply(std::move(payload), now_ms());
    const bool ok = code == hg::ProvisioningCode::Accepted;
    return send_json(request, ok ? "200 OK" : "422 Unprocessable Entity", std::string("{\"ok\":") + (ok ? "true" : "false") + ",\"result\":\"" + code_name(code) + "\"}");
}
