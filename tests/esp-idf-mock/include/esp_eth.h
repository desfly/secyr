#pragma once
#include "esp_err.h"
using esp_eth_handle_t = void*;

struct esp_eth_mac_t;
struct esp_eth_phy_t;
inline esp_err_t mock_eth_mac_del(esp_eth_mac_t*) { return ESP_OK; }
inline esp_err_t mock_eth_phy_del(esp_eth_phy_t*) { return ESP_OK; }

struct esp_eth_mac_t {
    esp_err_t (*del)(esp_eth_mac_t*){mock_eth_mac_del};
};
struct esp_eth_phy_t {
    esp_err_t (*del)(esp_eth_phy_t*){mock_eth_phy_del};
};
struct esp_eth_config_t { esp_eth_mac_t* mac; esp_eth_phy_t* phy; };
#define ETH_DEFAULT_CONFIG(mac,phy) esp_eth_config_t{mac,phy}
inline esp_err_t esp_eth_driver_install(const esp_eth_config_t*,esp_eth_handle_t* h){*h=(void*)1;return ESP_OK;}
inline esp_err_t esp_eth_driver_uninstall(esp_eth_handle_t){return ESP_OK;}
inline esp_err_t esp_eth_start(esp_eth_handle_t){return ESP_OK;}
inline esp_err_t esp_eth_stop(esp_eth_handle_t){return ESP_OK;}
inline void* esp_eth_new_netif_glue(esp_eth_handle_t){return (void*)1;}
