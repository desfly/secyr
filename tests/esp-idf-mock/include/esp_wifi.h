#pragma once
#include "esp_err.h"
#include <cstdint>

struct wifi_init_config_t {};
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}

enum wifi_mode_t { WIFI_MODE_AP = 2 };
enum wifi_interface_t { WIFI_IF_AP = 1 };
enum wifi_auth_mode_t { WIFI_AUTH_OPEN = 0 };

struct wifi_ap_config_t {
    std::uint8_t ssid[32]{};
    std::uint8_t password[64]{};
    std::uint8_t ssid_len{};
    std::uint8_t channel{};
    wifi_auth_mode_t authmode{WIFI_AUTH_OPEN};
    std::uint8_t ssid_hidden{};
    std::uint8_t max_connection{};
    std::uint16_t beacon_interval{};
};

struct wifi_config_t { wifi_ap_config_t ap{}; };

inline esp_err_t esp_wifi_init(const wifi_init_config_t*) { return ESP_OK; }
inline esp_err_t esp_wifi_set_mode(wifi_mode_t) { return ESP_OK; }
inline esp_err_t esp_wifi_set_config(wifi_interface_t, const wifi_config_t*) { return ESP_OK; }
inline esp_err_t esp_wifi_start() { return ESP_OK; }
