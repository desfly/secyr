#include "hg_service_http.hpp"
#include "homeguard/service_readiness.hpp"

namespace homeguard::idf {
namespace {
ServiceHttp* self_from(httpd_req_t* request) {
    return static_cast<ServiceHttp*>(request->user_ctx);
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
