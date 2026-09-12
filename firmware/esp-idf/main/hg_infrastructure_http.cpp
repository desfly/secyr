#include "hg_infrastructure_http.hpp"
#include "hg_hardware_bootstrap.hpp"
#include "hg_ble_transport.hpp"
#include "hg_request_auth.hpp"
#include "homeguard/hardware_runtime.hpp"

#include "esp_netif.h"
#include "esp_wifi.h"

#include <cstddef>
#include <cstring>
#include <sstream>
#include <string>

namespace homeguard::idf {
namespace {

std::string json_escape(const char* value)
{
    std::string out;
    if (value == nullptr) return out;
    for (const unsigned char ch : std::string(value)) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (ch >= 0x20U) out.push_back(static_cast<char>(ch));
                break;
        }
    }
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
    const bool ethernet_online = ethernet.initialized && ethernet.link_up && ethernet.has_ip;

    wifi_ap_record_t wifi_ap{};
    const bool wifi_connected = esp_wifi_sta_get_ap_info(&wifi_ap) == ESP_OK;
    std::string wifi_ip;
    if (wifi_connected) {
        if (auto* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"); netif != nullptr) {
            esp_netif_ip_info_t info{};
            if (esp_netif_get_ip_info(netif, &info) == ESP_OK && info.ip.addr != 0U) {
                char buffer[16]{};
                std::snprintf(buffer, sizeof(buffer), IPSTR, IP2STR(&info.ip));
                wifi_ip = buffer;
            }
        }
    }
    const bool wifi_online = wifi_connected && !wifi_ip.empty();
    const auto ssid_length = wifi_connected
        ? strnlen(reinterpret_cast<const char*>(wifi_ap.ssid), sizeof(wifi_ap.ssid))
        : 0U;
    const std::string wifi_ssid = wifi_connected
        ? std::string(reinterpret_cast<const char*>(wifi_ap.ssid), ssid_length)
        : std::string{};

    const bool ble_connected = BleTransport::active_connection();
    const char* preferred = ethernet_online ? "ethernet" :
                            wifi_online ? "wifi" :
                            ble_connected ? "ble" : "offline";

    std::ostringstream output;
    output << "{\"ok\":true,\"preferred\":\"" << preferred << "\",";
    output << "\"ethernet\":{";
    output << "\"initialized\":" << (ethernet.initialized ? "true" : "false") << ",";
    output << "\"online\":" << (ethernet_online ? "true" : "false") << ",";
    output << "\"linkUp\":" << (ethernet.link_up ? "true" : "false") << ",";
    output << "\"hasIp\":" << (ethernet.has_ip ? "true" : "false") << ",";
    output << "\"ip\":\"" << json_escape(ethernet.ipv4.c_str()) << "\"},";
    output << "\"wifi\":{";
    output << "\"online\":" << (wifi_online ? "true" : "false") << ",";
    output << "\"connected\":" << (wifi_connected ? "true" : "false") << ",";
    output << "\"ssid\":\"" << json_escape(wifi_ssid.c_str()) << "\",";
    output << "\"ip\":\"" << json_escape(wifi_ip.c_str()) << "\",";
    output << "\"rssi\":" << (wifi_connected ? static_cast<int>(wifi_ap.rssi) : 0) << "},";
    output << "\"ble\":{";
    output << "\"online\":" << (ble_connected ? "true" : "false") << ",";
    output << "\"connected\":" << (ble_connected ? "true" : "false") << ",";
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
