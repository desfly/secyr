#pragma once
#include "esp_err.h"
struct esp_netif_t {};
struct esp_netif_config_t {};
inline esp_err_t esp_netif_init(){return ESP_OK;}
inline esp_netif_t* esp_netif_new(const esp_netif_config_t*){static esp_netif_t n;return &n;}
inline esp_err_t esp_netif_attach(esp_netif_t*,void*){return ESP_OK;}
#define ESP_NETIF_DEFAULT_ETH() esp_netif_config_t{}
