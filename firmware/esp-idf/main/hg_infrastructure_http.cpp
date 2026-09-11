#include "hg_infrastructure_http.hpp"
#include "hg_hardware_bootstrap.hpp"
#include "hg_ble_transport.hpp"
#include "hg_request_auth.hpp"
#include "homeguard/hardware_runtime.hpp"

#include <cstddef>
#include <sstream>

namespace homeguard::idf {

esp_err_t InfrastructureHttp::register_handlers(
    httpd_handle_t server,
    HardwareBootstrap* hardware,
    homeguard::AccessControl* access_control)
{
    if (server == nullptr || hardware == nullptr || access_control == nullptr) return ESP_ERR_INVALID_ARG;
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

    const httpd_uri_t connectivity_route{
        .uri = "/api/v1/connectivity/status",
        .method = HTTP_GET,
        .handler = &InfrastructureHttp::connectivity_get,
        .user_ctx = this,
    };
    return httpd_register_uri_handler(server, &connectivity_route);
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

esp_err_t InfrastructureHttp::connectivity_get(httpd_req_t* request)
{
    if (request == nullptr || request->user_ctx == nullptr) return ESP_ERR_INVALID_ARG;
    auto* self = static_cast<InfrastructureHttp*>(request->user_ctx);
    if (self->hardware_ == nullptr || self->access_control_ == nullptr) return ESP_FAIL;
    if (!request_auth::authenticated(request, *self->access_control_)) {
        return request_auth::send_login_required(request);
    }
    if (!request_auth::authenticated_admin(request, *self->access_control_)) {
        return request_auth::send_admin_required(request);
    }

    const auto& ethernet = self->hardware_->ethernet().status();
    std::ostringstream output;
    output << "{\"ok\":true,\"ethernet\":{";
    output << "\"initialized\":" << (ethernet.initialized ? "true" : "false") << ",";
    output << "\"linkUp\":" << (ethernet.link_up ? "true" : "false") << ",";
    output << "\"hasIp\":" << (ethernet.has_ip ? "true" : "false") << ",";
    output << "\"ip\":\"" << ethernet.ipv4 << "\"},";
    output << "\"ble\":{";
    output << "\"connected\":" << (BleTransport::active_connection() ? "true" : "false") << ",";
    output << "\"ip\":\"—\"}}";

    const auto body = output.str();
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
