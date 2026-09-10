#include "hg_infrastructure_http.hpp"
#include "hg_hardware_bootstrap.hpp"
#include "hg_ble_transport.hpp"
#include "hg_request_auth.hpp"
#include "homeguard/hardware_runtime.hpp"

#include "esp_mac.h"
#include "esp_timer.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace homeguard::idf {
namespace {

std::string dashboard_runtime_json(HardwareBootstrap& hardware)
{
    const auto& ethernet = hardware.ethernet().status();

    std::uint8_t bt_mac[6]{};
    char bt_address[18]{};
    if (esp_read_mac(bt_mac, ESP_MAC_BT) == ESP_OK) {
        std::snprintf(
            bt_address,
            sizeof(bt_address),
            "%02X:%02X:%02X:%02X:%02X:%02X",
            bt_mac[0], bt_mac[1], bt_mac[2], bt_mac[3], bt_mac[4], bt_mac[5]);
    }

    const bool ble_available = ble_transport_ready();
    const bool ble_connected = ble_transport_connected();
    const char* lan_state = ethernet.has_ip ? "connected" : (ethernet.link_up ? "link" : "offline");

    std::string out = "{\"uptimeMs\":" +
        std::to_string(static_cast<std::uint64_t>(esp_timer_get_time() / 1000LL)) +
        ",\"lan\":{\"state\":\"" + lan_state +
        "\",\"name\":\"Ethernet\",\"ip\":\"" + ethernet.ipv4 +
        "\"},\"ble\":{\"ready\":" + (ble_connected ? "true" : "false") +
        ",\"available\":" + (ble_available ? "true" : "false") +
        ",\"connected\":" + (ble_connected ? "true" : "false") +
        ",\"name\":\"HomeGuard-S3\",\"address\":\"" + bt_address + "\"}}";
    return out;
}

}  // namespace

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
    return httpd_register_uri_handler(server, &analog_route);
}

esp_err_t InfrastructureHttp::status_get(httpd_req_t* request)
{
    if (request == nullptr || request->user_ctx == nullptr) return ESP_ERR_INVALID_ARG;
    auto* self = static_cast<InfrastructureHttp*>(request->user_ctx);
    if (self->hardware_ == nullptr || self->access_control_ == nullptr) return ESP_FAIL;
    if (!request_auth::authenticated(request, *self->access_control_)) {
        return request_auth::send_login_required(request);
    }

    auto body = hardware_runtime_json(self->hardware_->status());
    if (!body.empty() && body.back() == '}') {
        body.pop_back();
        body += ",\"dashboard\":" + dashboard_runtime_json(*self->hardware_) + "}";
    }
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
