#pragma once

#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ESP_MAC_WIFI_STA = 0,
    ESP_MAC_WIFI_SOFTAP,
    ESP_MAC_BT,
    ESP_MAC_ETH
} esp_mac_type_t;

static inline esp_err_t esp_read_mac(uint8_t* mac, esp_mac_type_t type)
{
    if (mac == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    mac[0] = 0x02;
    mac[1] = 0x00;
    mac[2] = 0x00;
    mac[3] = 0x00;
    mac[4] = 0x00;
    mac[5] = static_cast<uint8_t>(type);
    return ESP_OK;
}

#ifdef __cplusplus
}
#endif
