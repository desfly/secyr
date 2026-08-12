#include "hg_lan_discovery_http.hpp"

#include "lwip/etharp.h"
#include "lwip/ip4_addr.h"

#include <cstdio>
#include <string>

namespace homeguard::idf {
namespace {

esp_err_t send_json(httpd_req_t* request, const std::string& body)
{
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, body.c_str(), static_cast<ssize_t>(body.size()));
}

}  // namespace

esp_err_t LanDiscoveryHttp::register_handlers(httpd_handle_t server)
{
    if (server == nullptr) return ESP_ERR_INVALID_ARG;
    const httpd_uri_t route{
        .uri = "/api/v1/network/lan-scan",
        .method = HTTP_GET,
        .handler = &LanDiscoveryHttp::scan_get,
        .user_ctx = this,
    };
    return httpd_register_uri_handler(server, &route);
}

esp_err_t LanDiscoveryHttp::scan_get(httpd_req_t* request)
{
    if (request == nullptr) return ESP_ERR_INVALID_ARG;

    std::string out = "{\"ok\":true,\"method\":\"arp\",\"devices\":[";
    bool first = true;
    for (std::size_t i = 0; i < ETHARP_TABLE_SIZE; ++i) {
        ip4_addr_t* ip = nullptr;
        struct netif* netif = nullptr;
        struct eth_addr* mac = nullptr;
        if (!etharp_get_entry(i, &ip, &netif, &mac) || ip == nullptr || mac == nullptr) continue;

        char ipbuf[IP4ADDR_STRLEN_MAX]{};
        if (ip4addr_ntoa_r(ip, ipbuf, sizeof(ipbuf)) == nullptr) continue;

        char macbuf[18]{};
        std::snprintf(macbuf, sizeof(macbuf), "%02X:%02X:%02X:%02X:%02X:%02X",
                      mac->addr[0], mac->addr[1], mac->addr[2],
                      mac->addr[3], mac->addr[4], mac->addr[5]);

        if (!first) out += ',';
        first = false;
        out += "{\"ip\":\"";
        out += ipbuf;
        out += "\",\"mac\":\"";
        out += macbuf;
        out += "\",\"hostname\":\"\",\"online\":true}";
    }
    out += "]}";
    return send_json(request, out);
}

}  // namespace homeguard::idf
