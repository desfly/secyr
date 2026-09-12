#include "hg_connectivity_http.hpp"

#include "hg_ble_transport.hpp"
#include "hg_cloud_link.hpp"
#include "hg_hardware_bootstrap.hpp"
#include "hg_request_auth.hpp"
#include "homeguard/access_control.hpp"

#include "esp_netif.h"
#include "esp_wifi.h"

#include <cstdio>
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

std::string ipv4_text(const esp_ip4_addr_t& address)
{
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), IPSTR, IP2STR(&address));
    return buffer;
}

esp_err_t send_json(httpd_req_t* request, const std::string& body)
{
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

}  // namespace

esp_err_t ConnectivityHttp::register_handlers(
    httpd_handle_t server,
    HardwareBootstrap* hardware,
    NetworkHttp* network,
    BleTransport* ble,
    CloudLink* cloud,
    homeguard::AccessControl* access_control)
{
    if (server == nullptr || hardware == nullptr || network == nullptr || ble == nullptr ||
        cloud == nullptr || access_control == nullptr) return ESP_ERR_INVALID_ARG;
    hardware_ = hardware;
    network_ = network;
    ble_ = ble;
    cloud_ = cloud;
    access_control_ = access_control;

    const httpd_uri_t route{
        .uri = "/api/v1/connectivity/status",
        .method = HTTP_GET,
        .handler = &ConnectivityHttp::status_get,
        .user_ctx = this,
    };
    return httpd_register_uri_handler(server, &route);
}

esp_err_t ConnectivityHttp::status_get(httpd_req_t* request)
{
    if (request == nullptr || request->user_ctx == nullptr) return ESP_ERR_INVALID_ARG;
    auto* self = static_cast<ConnectivityHttp*>(request->user_ctx);
    if (self->hardware_ == nullptr || self->ble_ == nullptr || self->cloud_ == nullptr ||
        self->access_control_ == nullptr) return ESP_FAIL;
    if (!request_auth::authenticated(request, *self->access_control_)) {
        return request_auth::send_login_required(request);
    }

    const auto ethernet = self->hardware_->ethernet().status();
    const bool ethernet_online = ethernet.initialized && ethernet.link_up && ethernet.has_ip;

    wifi_ap_record_t ap{};
    const bool wifi_connected = esp_wifi_sta_get_ap_info(&ap) == ESP_OK;
    std::string wifi_ip;
    if (wifi_connected) {
        if (auto* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"); netif != nullptr) {
            esp_netif_ip_info_t info{};
            if (esp_netif_get_ip_info(netif, &info) == ESP_OK && info.ip.addr != 0U) {
                wifi_ip = ipv4_text(info.ip);
            }
        }
    }
    const bool wifi_online = wifi_connected && !wifi_ip.empty();
    const std::string ssid = wifi_connected
        ? std::string(reinterpret_cast<const char*>(ap.ssid), strnlen(reinterpret_cast<const char*>(ap.ssid), sizeof(ap.ssid)))
        : std::string{};

    const bool ble_connected = self->ble_->connected();
    const bool cloud_connected = self->cloud_->connected();
    const bool cloud_configured = self->cloud_->configured();

    const char* preferred = ethernet_online ? "ethernet" :
                            wifi_online ? "wifi" :
                            ble_connected ? "ble" :
                            cloud_connected ? "cloud" : "offline";

    std::string body = "{\"ok\":true,\"preferred\":\"";
    body += preferred;
    body += "\",\"channels\":{";
    body += "\"ethernet\":{\"available\":" + std::string(ethernet.initialized ? "true" : "false") +
            ",\"online\":" + (ethernet_online ? "true" : "false") +
            ",\"linkUp\":" + (ethernet.link_up ? "true" : "false") +
            ",\"hasIp\":" + (ethernet.has_ip ? "true" : "false") +
            ",\"ip\":\"" + json_escape(ethernet.ipv4.c_str()) + "\"},";
    body += "\"wifi\":{\"available\":true,\"online\":" + std::string(wifi_online ? "true" : "false") +
            ",\"connected\":" + (wifi_connected ? "true" : "false") +
            ",\"ssid\":\"" + json_escape(ssid.c_str()) + "\"" +
            ",\"ip\":\"" + json_escape(wifi_ip.c_str()) + "\"" +
            ",\"rssi\":" + std::to_string(wifi_connected ? static_cast<int>(ap.rssi) : 0) + "},";
    body += "\"ble\":{\"available\":true,\"online\":" + std::string(ble_connected ? "true" : "false") +
            ",\"connected\":" + (ble_connected ? "true" : "false") +
            ",\"connectionEpoch\":" + std::to_string(self->ble_->connection_epoch()) + "},";
    body += "\"cloud\":{\"available\":" + std::string(cloud_configured ? "true" : "false") +
            ",\"online\":" + (cloud_connected ? "true" : "false") +
            ",\"configured\":" + (cloud_configured ? "true" : "false") +
            ",\"deviceId\":\"" + json_escape(self->cloud_->device_id()) + "\"" +
            ",\"connectCount\":" + std::to_string(self->cloud_->connect_count()) +
            ",\"disconnectCount\":" + std::to_string(self->cloud_->disconnect_count()) + "}}}";

    return send_json(request, body);
}

}  // namespace homeguard::idf
