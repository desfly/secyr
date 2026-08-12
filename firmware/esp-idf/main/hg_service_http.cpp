#include "hg_service_http.hpp"
#include "homeguard/service_readiness.hpp"

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
    hg::SystemEventBus* bus)
{
    if (server == nullptr || store == nullptr || hardware == nullptr || commissioning == nullptr ||
        readiness == nullptr || bus == nullptr) return ESP_ERR_INVALID_ARG;
    store_ = store;
    hardware_ = hardware;
    commissioning_ = commissioning;
    readiness_ = readiness;
    bus_ = bus;

    const httpd_uri_t routes[] = {
        {.uri="/api/v1/service/readiness", .method=HTTP_GET, .handler=&ServiceHttp::readiness_get, .user_ctx=this},
        {.uri="/api/v1/service/invalidate", .method=HTTP_POST, .handler=&ServiceHttp::invalidate_post, .user_ctx=this},
    };
    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

esp_err_t ServiceHttp::send_json(httpd_req_t* request, const std::string& body) const {
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
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

    // Destructive service operations are fail-closed. They must never be
    // reachable through a direct HTTP call that bypasses the Web UI.
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
        return self->send_json(request, std::string{"{\"ok\":false,\"reason\":\""} +
            homeguard::to_string(decision) + "\"}");
    }

    const auto error = self->store_->erase_all();
    if (error != ESP_OK) {
        httpd_resp_set_status(request, "500 Internal Server Error");
        return self->send_json(request, "{\"ok\":false,\"reason\":\"nvs_erase_failed\"}");
    }

    *self->hardware_ = {};
    *self->commissioning_ = {};
    *self->readiness_ = hg::evaluate_boot_readiness({nullptr, nullptr});
    if (self->bus_ != nullptr) {
        self->bus_->publish({hg::SystemEventType::ConfigChanged, 0, 0, 0, 5000});
    }
    return self->send_json(request,
        "{\"ok\":true,\"outputsAllowed\":false,\"reason\":\"commissioning_invalidated\"}");
}

}  // namespace homeguard::idf
