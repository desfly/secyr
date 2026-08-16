#pragma once
#include "esp_eth.h"
using esp_eth_netif_glue_handle_t = void*;
inline esp_err_t esp_eth_del_netif_glue(esp_eth_netif_glue_handle_t){return ESP_OK;}
