#include "hg_service_http.hpp"
#include "hg_factory_reset.hpp"
#include "homeguard/controller_config_backup.hpp"
#include "homeguard/service_readiness.hpp"

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstddef>
#include <string>

namespace homeguard::idf {
namespace {
ServiceHttp* self_from(httpd_req_t* request) {
    return static_cast<ServiceHttp*>(request->user_ctx);
}

bool parse_json_string(const std::string& body, const char* key, std::string& value) {
    const std::string marker = std::string{"\""} + key + "\"";
    auto pos = body.find(marker);
    if (pos == std::string::npos) return false;
    pos = body.find(':', pos + marker.size());
    if (pos == std::string::npos) return false;
    pos = body.find('"', pos + 1U);
    if (pos == std::string::npos) return false;
    const auto end = body.find('"', pos + 1U);
    if (end == std::string::npos) return false;
    value.assign(body, pos + 1U, end - pos - 1U);
    return true;
}

bool read_body(httpd_req_t* request, std::size_t limit, std::string& body) {
    if (request == nullptr || request->content_len == 0 || request->content_len > limit) return false;
    body.assign(request->content_len, '\0');
    std::size_t offset = 0;
    while (offset < body.size()) {
        const auto received = httpd_req_recv(request, body.data() + offset, body.size() - offset);
        if (received <= 0) return false;
        offset += static_cast<std::size_t>(received);
    }
    return true;
}

void delayed_restart(void*) {
    vTaskDelay(pdMS_TO_TICKS(350));
    esp_restart();
}
}

esp_err_t ServiceHttp::register_handlers(
    httpd_handle_t server,
    CommissioningNvsStore* store,
    hg::HardwareVerificationRecord* hardware,
    hg::CommissioningPersistentState* commissioning,
    hg::BootReadinessReport* readiness,
    hg::SystemEventBus* bus,
    NvsConfigStore* config_store,
    hg::ControllerConfig* controller_config)
{
    if (server == nullptr || store == nullptr || hardware == nullptr || commissioning == nullptr ||
        readiness == nullptr || bus == nullptr || config_store == nullptr || controller_config == nullptr) return ESP_ERR_INVALID_ARG;
    store_ = store;
    hardware_ = hardware;
    commissioning_ = commissioning;
    readiness_ = readiness;
    bus_ = bus;
    config_store_ = config_store;
    controller_config_ = controller_config;

    const httpd_uri_t routes[] = {
        {.uri="/api/v1/service/readiness", .method=HTTP_GET, .handler=&ServiceHttp::readiness_get, .user_ctx=this},
        {.uri="/api/v1/service/invalidate", .method=HTTP_POST, .handler=&ServiceHttp::invalidate_post, .user_ctx=this},
        {.uri="/api/v1/system/factory-reset", .method=HTTP_POST, .handler=&ServiceHttp::factory_reset_post, .user_ctx=this},
        {.uri="/api/v1/system/config-export", .method=HTTP_POST, .handler=&ServiceHttp::config_export_post, .user_ctx=this},
        {.uri="/api/v1/system/config-import", .method=HTTP_POST, .handler=&ServiceHttp::config_import_post, .user_ctx=this},
    };
    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

esp_err_t ServiceHttp::send_json(httpd_req_t* request, const std::string& body) const {
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

bool ServiceHttp::authorize_admin(httpd_req_t* request, const std::string& body, const char* command) const {
    if (request == nullptr || access_control_ == nullptr) return false;
    std::string actor;
    std::string credential;
    if (!parse_json_string(body, "actor", actor) || !parse_json_string(body, "credential", credential)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
        return false;
    }
    const auto decision = access_control_->authorize(actor, credential, command);
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        send_json(request, std::string{"{\"ok\":false,\"reason\":\""} + homeguard::to_string(decision) + "\"}");
        return false;
    }
    return true;
}

esp_err_t ServiceHttp::readiness_get(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr) return ESP_FAIL;
    const auto snapshot = hg::make_service_readiness_snapshot(self->hardware_, self->commissioning_);
    return self->send_json(request, hg::service_readiness_json(snapshot));
}

esp_err_t ServiceHttp::invalidate_post(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->store_ == nullptr || self->hardware_ == nullptr ||
        self->commissioning_ == nullptr || self->readiness_ == nullptr) return ESP_FAIL;
    if (self->access_control_ == nullptr) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"access_unavailable\"}");
    }
    std::string body;
    if (!read_body(request, 384U, body)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }
    std::string actor;
    std::string credential;
    if (!parse_json_string(body, "actor", actor) || !parse_json_string(body, "credential", credential)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }
    const auto decision = self->access_control_->authorize(actor, credential, "system.service.invalidate");
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return self->send_json(request, std::string{"{\"ok\":false,\"reason\":\""} + homeguard::to_string(decision) + "\"}");
    }
    const auto error = self->store_->erase_all();
    if (error != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"nvs_erase_failed\"}");
    }
    *self->hardware_ = {};
    *self->commissioning_ = {};
    *self->readiness_ = hg::evaluate_boot_readiness({nullptr, nullptr});
    if (self->bus_ != nullptr) self->bus_->publish({hg::SystemEventType::ConfigChanged, 0, 0, 0, 5000});
    return self->send_json(request, "{\"ok\":true,\"outputsAllowed\":false,\"reason\":\"commissioning_invalidated\"}");
}

esp_err_t ServiceHttp::config_export_post(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->controller_config_ == nullptr || self->access_control_ == nullptr) return ESP_FAIL;
    std::string body;
    if (!read_body(request, 384U, body)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }
    if (!self->authorize_admin(request, body, "system.config.export")) return ESP_OK;
    httpd_resp_set_hdr(request, "Content-Disposition", "attachment; filename=homeguard-s3-controller-settings.json");
    return self->send_json(request, hg::ControllerConfigBackup::encode(*self->controller_config_));
}

esp_err_t ServiceHttp::config_import_post(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->controller_config_ == nullptr || self->config_store_ == nullptr || self->access_control_ == nullptr) return ESP_FAIL;
    std::string body;
    if (!read_body(request, 8192U, body)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"invalid_or_oversized_backup\"}");
    }
    if (!self->authorize_admin(request, body, "system.config.import")) return ESP_OK;

    hg::ControllerConfig candidate{};
    std::string error;
    if (!hg::ControllerConfigBackup::decode(body, candidate, error)) {
        httpd_resp_set_status(request, "422 Unprocessable Entity");
        return self->send_json(request, std::string{"{\"ok\":false,\"reason\":\""} + error + "\"}");
    }
    if (!self->config_store_->save(candidate)) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"config_persist_failed\"}");
    }
    *self->controller_config_ = candidate;
    if (self->bus_ != nullptr) self->bus_->publish({hg::SystemEventType::ConfigChanged, 0, 0, 0, 6000});
    return self->send_json(request, "{\"ok\":true,\"reason\":\"config_imported\",\"rebootRequired\":false}");
}

esp_err_t ServiceHttp::factory_reset_post(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->access_control_ == nullptr) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return self != nullptr ? self->send_json(request, "{\"ok\":false,\"reason\":\"access_unavailable\"}") : ESP_FAIL;
    }
    std::string body;
    if (!read_body(request, 512U, body)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }
    std::string actor;
    std::string credential;
    std::string confirmation;
    if (!parse_json_string(body, "actor", actor) || !parse_json_string(body, "credential", credential) ||
        !parse_json_string(body, "confirm", confirmation)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"confirmation_required\"}");
    }
    if (confirmation != "ERASE_ALL") {
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"confirmation_mismatch\"}");
    }
    const auto decision = self->access_control_->authorize(actor, credential, "system.factory_reset");
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return self->send_json(request, std::string{"{\"ok\":false,\"reason\":\""} + homeguard::to_string(decision) + "\"}");
    }
    const auto report = FactoryResetManager{}.erase_mutable_state();
    if (!report.ok()) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return self->send_json(request,
            std::string{"{\"ok\":false,\"reason\":\"erase_failed\",\"access\":"} + std::to_string(report.access) +
            ",\"wifi\":" + std::to_string(report.wifi) + ",\"cloud\":" + std::to_string(report.cloud) +
            ",\"controllerConfig\":" + std::to_string(report.controller_config) + ",\"provisioning\":" +
            std::to_string(report.provisioning) + ",\"commissioning\":" + std::to_string(report.commissioning) + "}");
    }
    httpd_resp_set_status(request, "202 Accepted");
    const auto response = self->send_json(request, "{\"ok\":true,\"reason\":\"factory_reset_started\",\"rebooting\":true}");
    if (xTaskCreate(&delayed_restart, "hg_factory_reboot", 1536, nullptr, 5, nullptr) != pdPASS) esp_restart();
    return response;
}

}  // namespace homeguard::idf
