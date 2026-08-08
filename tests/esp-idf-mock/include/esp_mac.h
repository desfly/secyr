#pragma once
#include "esp_err.h"
#include <cstdint>
enum esp_mac_type_t { ESP_MAC_WIFI_STA = 0, ESP_MAC_WIFI_SOFTAP = 1 };
inline esp_err_t esp_read_mac(std::uint8_t* mac, esp_mac_type_t type) {
    if (mac) {
        mac[0]=0xAC; mac[1]=0xA7; mac[2]=0x04; mac[3]=0x1D; mac[4]=0xA7;
        mac[5]=(type == ESP_MAC_WIFI_SOFTAP) ? 0x11 : 0x10;
    }
    return ESP_OK;
}
