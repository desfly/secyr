#include "hg_infrastructure_http.hpp"
#include "hg_hardware_bootstrap.hpp"
#include "hg_request_auth.hpp"
#include "hg_zone_http.hpp"
#include "hg_zone_monitor.hpp"
#include "homeguard/hardware_runtime.hpp"
#include "homeguard/system_model.hpp"

#include <cstddef>

namespace homeguard::idf {
namespace {
ZoneMonitor g_zone_monitor;
ZoneHttp g_zone_http;
bool g_zone_monitor_started = false;
}

esp_err_t InfrastructureHttp::register_handlers(
    httpd_handle_t server,
    HardwareBootstrap* hardware,
    homeguard::AccessControl* access_control,
    hg::SystemModel* system_model)
{
    if (server == nullptr || hardware == nullptr || access_control == nullptr || system_model == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    hardware_ = hardware;
    access_control_ = access_control;

    const httpd_uri_t status_route{
        .uri = "/api/v1/hardware/status",
        .method = HTTP_GET,
        .handler = &InfrastructureHttp::status_get,
        .user_ctx = this,
    };
    auto error = httpd_register_uri_handler(server, &status_route);
    if (error != ESP_OK) return error;

    const httpd_uri_t analog_route{
        .uri = "/api/v1/hardware/analog",
        .method = HTTP_GET,
        .handler = &InfrastructureHttp::analog_get,
        .user_ctx = this,
    };
    error = httpd_register_uri_handler(server, &analog_route);
    if (error != ESP_OK) return error;

    if (!g_zone_monitor_started) {
        error = g_zone_monitor.start(&hardware_->zone_adc(), &hardware_->telemetry_adc(), system_model);
        if (error != ESP_OK) return error;
        g_zone_monitor_started = true;
    }

    return g_zone_http.register_handlers(server, &g_zone_monitor, access_control);
}

esp_err_t InfrastructureHttp::status_get(httpd_req_t* request)
{
    if (request == nullptr || request->user_ctx == nullptr) return ESP_ERR_INVALID_ARG;
    auto* self = static_cast<InfrastructureHttp*>(request->user_ctx);
    if (self->hardware_ == nullptr || self->access_control_ == nullptr) return ESP_FAIL;
    if (!request_auth::authenticated(request, *self->access_control_)) {
        return request_auth::send_login_required(request);
    }

    const auto body = hardware_runtime_json(self->hardware_->status());
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.c_str(), body.size());
}

esp_err_t InfrastructureHttp::analog_get(httpd_req_t* request)
{
    if (request == nullptr || request->user_ctx == nullptr) return ESP_ERR_INVALID_ARG;
    auto* self = static_cast<InfrastructureHttp*>(request->user_ctx);
    if (self->hardware_ == nullptr || self->access_control_ == nullptr) return ESP_FAIL;
    if (!request_auth::authenticated(request, *self->access_control_)) {
        return request_auth::send_login_required(request);
    }

    const auto body = self->hardware_->analog_snapshot_json();
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.c_str(), body.size());
}

esp_err_t InfrastructureHttp::rgb_test_post(httpd_req_t* request)
{
    if (request == nullptr) return ESP_ERR_INVALID_ARG;
    httpd_resp_set_status(request, "404 Not Found");
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(
        request,
        "{\"ok\":false,\"reason\":\"remote_rgb_test_disabled\"}",
        HTTPD_RESP_USE_STRLEN);
}

}  // namespace homeguard::idf
