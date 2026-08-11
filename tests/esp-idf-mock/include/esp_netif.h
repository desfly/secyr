#pragma once

#include "esp_err.h"
#include "esp_netif_ip_addr.h"

struct esp_netif_t {};
struct esp_netif_config_t {};

inline esp_err_t esp_netif_init(){return ESP_OK;}
inline esp_netif_t* esp_netif_new(const esp_netif_config_t*){static esp_netif_t n;return &n;}
inline esp_err_t esp_netif_attach(esp_netif_t*,void*){return ESP_OK;}
inline esp_netif_t* esp_netif_create_default_wifi_ap(){static esp_netif_t n;return &n;}
inline esp_netif_t* esp_netif_create_default_wifi_sta(){static esp_netif_t n;return &n;}
inline void esp_netif_destroy_default_wifi(esp_netif_t*){}
inline esp_netif_t* esp_netif_get_handle_from_ifkey(const char*){static esp_netif_t n;return &n;}
inline esp_err_t esp_netif_get_ip_info(esp_netif_t*, esp_netif_ip_info_t*){return ESP_OK;}

#define ESP_NETIF_DEFAULT_ETH() esp_netif_config_t{}
