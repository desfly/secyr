#include "hg_service_http.hpp"
#include "homeguard/service_readiness.hpp"

#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

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
}

esp_err_t ServiceHttp::register_handlers(
    httpd_handle_t server,
    CommissioningNvsStore* store,
    hg::HardwareVerificationRecord* hardware,
    hg::CommissioningPersistentState* commissioning,
    hg::BootReadinessReport* readiness,
    hg::PhysicalOutputRuntime* physical_outputs,
    hg::SystemEventBus* bus,
    std::mutex* control_state_mutex)
{
    if (server == nullptr || store == nullptr || hardware == nullptr || commissioning == nullptr ||
        readiness == nullptr || physical_outputs == nullptr || bus == nullptr ||
        control_state_mutex == nullptr) return ESP_ERR_INVALID_ARG;
    store_ = store;
    hardware_ = hardware;
    commissioning_ = commissioning;
    readiness_ = readiness;
    physical_outputs_ = physical_outputs;
    bus_ = bus;
    control_state_mutex_ = control_state_mutex;

    const httpd_uri_t routes[] = {
        {.uri="/api/v1/service/readiness", .method=HTTP_GET, .handler=&ServiceHttp::readiness_get, .user_ctx=this},
        {.uri="/api/v1/service/invalidate", .method=HTTP_POST, .handler=&ServiceHttp::invalidate_post, .user_ctx=this},
        {.uri="/api/v1/service/factory-reset", .method=HTTP_POST, .handler=&ServiceHttp::factory_reset_post, .user_ctx=this},
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

esp_err_t ServiceHttp::readiness_get(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->control_state_mutex_ == nullptr) return ESP_FAIL;

    hg::ServiceReadinessSnapshot snapshot{};
    {
        std::scoped_lock lock(*self->control_state_mutex_);
        snapshot = hg::make_service_readiness_snapshot(self->hardware_, self->commissioning_);
    }
    return self->send_json(request, hg::service_readiness_json(snapshot));
}

esp_err_t ServiceHttp::invalidate_post(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->store_ == nullptr || self->hardware_ == nullptr ||
        self->commissioning_ == nullptr || self->readiness_ == nullptr ||
        self->physical_outputs_ == nullptr || self->control_state_mutex_ == nullptr) return ESP_FAIL;

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
    credential.assign(credential.size(), '\0');
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return self->send_json(request, std::string{"{\"ok\":false,\"reason\":\""} +
            homeguard::to_string(decision) + "\"}");
    }

    // Lock order for destructive service is physical -> control-state. Sticky
    // output lockout is completed before mutable readiness records are touched.
    if (!self->physical_outputs_->lockout_fail_closed()) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"output_safe_failed\"}");
    }

    const auto error = self->store_->erase_all();
    if (error != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"nvs_erase_failed\"}");
    }

    {
        std::scoped_lock lock(*self->control_state_mutex_);
        *self->hardware_ = {};
        *self->commissioning_ = {};
        *self->readiness_ = hg::evaluate_boot_readiness({nullptr, nullptr});
    }
    if (self->bus_ != nullptr) {
        self->bus_->publish({hg::SystemEventType::ConfigChanged, 0, 0, 0, 5000});
    }
    return self->send_json(request,
        "{\"ok\":true,\"outputsAllowed\":false,\"reason\":\"commissioning_invalidated\"}");
}

esp_err_t ServiceHttp::factory_reset_post(httpd_req_t* request) {
    auto* self = self_from(request);
    if (self == nullptr || self->hardware_ == nullptr || self->commissioning_ == nullptr ||
        self->readiness_ == nullptr || self->physical_outputs_ == nullptr ||
        self->control_state_mutex_ == nullptr) return ESP_FAIL;

    if (self->access_control_ == nullptr) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"access_unavailable\"}");
    }

    std::string body;
    if (!read_body(request, 512U, body)) {
        httpd_resp_set_status(request, "401 Unauthorized");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"credential_required\"}");
    }

    std::string actor;
    std::string credential;
    std::string confirmation;
    if (!parse_json_string(body, "actor", actor) ||
        !parse_json_string(body, "credential", credential) ||
        !parse_json_string(body, "confirm", confirmation)) {
        httpd_resp_set_status(request, "400 Bad Request");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"confirmation_required\"}");
    }

    if (confirmation != "ERASE_ALL") {
        credential.assign(credential.size(), '\0');
        httpd_resp_set_status(request, "409 Conflict");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"confirmation_mismatch\"}");
    }

    const auto decision = self->access_control_->authorize(actor, credential, "system.factory_reset");
    credential.assign(credential.size(), '\0');
    if (decision != homeguard::AuditDecision::Allowed) {
        httpd_resp_set_status(request, "403 Forbidden");
        return self->send_json(request, std::string{"{\"ok\":false,\"reason\":\""} +
            homeguard::to_string(decision) + "\"}");
    }

    if (!self->physical_outputs_->lockout_fail_closed()) {
        httpd_resp_set_status(request, "503 Service Unavailable");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"output_safe_failed\"}");
    }

    const auto erase_error = nvs_flash_erase();
    if (erase_error != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"factory_reset_erase_failed\"}");
    }

    {
        std::scoped_lock lock(*self->control_state_mutex_);
        *self->hardware_ = {};
        *self->commissioning_ = {};
        *self->readiness_ = hg::evaluate_boot_readiness({nullptr, nullptr});
    }

    const auto response_error = self->send_json(request,
        "{\"ok\":true,\"state\":\"restarting\",\"factoryReset\":true,\"outputsAllowed\":false}");

    vTaskDelay(pdMS_TO_TICKS(150));
    esp_restart();
    return response_error;
}

}  // namespace homeguard::idf
