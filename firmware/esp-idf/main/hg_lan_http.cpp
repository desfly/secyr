#include "hg_lan_http.hpp"

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/etharp.h"
#include "lwip/inet.h"
#include "lwip/netif.h"
#include "lwip/sockets.h"

#include <cstdint>
#include <cstdio>
#include <string>

namespace homeguard::idf {
namespace {

constexpr std::int64_t kActiveScanCooldownUs = 3'000'000;
std::int64_t g_last_active_scan_us = -kActiveScanCooldownUs;

LanHttp* self_from(httpd_req_t* request)
{
    return request == nullptr ? nullptr : static_cast<LanHttp*>(request->user_ctx);
}

std::string ip_to_string(const ip4_addr_t& address)
{
    char text[16]{};
    ip4addr_ntoa_r(&address, text, sizeof(text));
    return text;
}

std::string mac_to_string(const eth_addr& mac)
{
    char text[18]{};
    std::snprintf(text, sizeof(text), "%02X:%02X:%02X:%02X:%02X:%02X",
                  mac.addr[0], mac.addr[1], mac.addr[2], mac.addr[3], mac.addr[4], mac.addr[5]);
    return text;
}

bool stimulate_arp_cache()
{
    if (netif_default == nullptr) return false;
    const auto local = ip4_addr_get_u32(netif_ip4_addr(netif_default));
    if (local == 0) return false;

    const auto now_us = esp_timer_get_time();
    if (now_us - g_last_active_scan_us < kActiveScanCooldownUs) return false;
    // Reserve the cooldown before the expensive work so back-to-back requests
    // cannot all enter the synchronous /24 stimulus path.
    g_last_active_scan_us = now_us;

    const int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return false;

    const std::uint32_t base = local & PP_HTONL(0xFFFFFF00UL);
    const char payload = 0;
    for (std::uint32_t host = 1; host < 255; ++host) {
        const std::uint32_t candidate = base | PP_HTONL(host);
        if (candidate == local) continue;
        sockaddr_in target{};
        target.sin_family = AF_INET;
        target.sin_port = htons(9);
        target.sin_addr.s_addr = candidate;
        (void)sendto(sock, &payload, 1, MSG_DONTWAIT,
                     reinterpret_cast<const sockaddr*>(&target), sizeof(target));
    }
    close(sock);
    vTaskDelay(pdMS_TO_TICKS(250));
    return true;
}

esp_err_t send_json(httpd_req_t* request, const std::string& body)
{
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

}  // namespace

esp_err_t LanHttp::register_handlers(httpd_handle_t server)
{
    if (server == nullptr) return ESP_ERR_INVALID_ARG;
    const httpd_uri_t routes[] = {
        {.uri="/api/v1/lan/devices", .method=HTTP_GET, .handler=&LanHttp::devices_get, .user_ctx=this},
        {.uri="/api/v1/lan/scan", .method=HTTP_GET, .handler=&LanHttp::scan_get, .user_ctx=this},
    };
    for (const auto& route : routes) {
        const auto error = httpd_register_uri_handler(server, &route);
        if (error != ESP_OK) return error;
    }
    return ESP_OK;
}

esp_err_t LanHttp::devices_get(httpd_req_t* request)
{
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : send_json(request, self->devices_json(false));
}

esp_err_t LanHttp::scan_get(httpd_req_t* request)
{
    auto* self = self_from(request);
    return self == nullptr ? ESP_FAIL : send_json(request, self->devices_json(true));
}

std::string LanHttp::devices_json(bool active_scan) const
{
    if (netif_default == nullptr || ip4_addr_isany_val(*netif_ip4_addr(netif_default))) {
        return "{\"ok\":true,\"state\":\"offline\",\"devices\":[]}";
    }

    const bool scan_performed = active_scan && stimulate_arp_cache();

    const auto local = ip4_addr_get_u32(netif_ip4_addr(netif_default));
    const std::uint32_t base = local & PP_HTONL(0xFFFFFF00UL);
    std::string out = std::string{"{\"ok\":true,\"state\":\"connected\",\"activeScan\":"} +
                      (scan_performed ? "true" : "false") +
                      ",\"scanThrottled\":" + (active_scan && !scan_performed ? "true" : "false") +
                      ",\"devices\":[";
    bool first = true;
    for (std::uint32_t host = 1; host < 255; ++host) {
        ip4_addr_t target{};
        ip4_addr_set_u32(&target, base | PP_HTONL(host));
        eth_addr* mac = nullptr;
        const ip4_addr_t* cached_ip = nullptr;
        if (etharp_find_addr(netif_default, &target, &mac, &cached_ip) < 0 || mac == nullptr || cached_ip == nullptr) continue;
        if (!first) out += ',';
        first = false;
        out += "{\"ip\":\"" + ip_to_string(*cached_ip) + "\",\"mac\":\"" + mac_to_string(*mac) + "\"}";
    }
    out += "]}";
    return out;
}

}  // namespace homeguard::idf
