#pragma once

#include "esp_err.h"

#include <cstdint>
#include <cstring>

using wifi_interface_t = int;
using wifi_mode_t = int;
using wifi_auth_mode_t = int;

constexpr wifi_interface_t WIFI_IF_STA = 0;
constexpr wifi_interface_t WIFI_IF_AP = 1;
constexpr wifi_mode_t WIFI_MODE_STA = 1;
constexpr wifi_mode_t WIFI_MODE_AP = 2;
constexpr wifi_mode_t WIFI_MODE_APSTA = 3;
constexpr wifi_auth_mode_t WIFI_AUTH_OPEN = 0;
constexpr wifi_auth_mode_t WIFI_AUTH_WPA2_PSK = 3;

struct wifi_pmf_config_t {
    bool capable{};
    bool required{};
};

struct wifi_sta_threshold_t {
    wifi_auth_mode_t authmode{};
};

struct wifi_sta_config_t {
    std::uint8_t ssid[32]{};
    std::uint8_t password[64]{};
    wifi_sta_threshold_t threshold{};
    wifi_pmf_config_t pmf_cfg{};
};

struct wifi_ap_config_t {
    std::uint8_t ssid[32]{};
    std::uint8_t password[64]{};
    std::uint8_t ssid_len{};
    std::uint8_t channel{};
    wifi_auth_mode_t authmode{};
    std::uint8_t max_connection{};
    wifi_pmf_config_t pmf_cfg{};
};

struct wifi_config_t {
    wifi_sta_config_t sta{};
    wifi_ap_config_t ap{};
};

struct wifi_init_config_t {};
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}

struct wifi_scan_config_t {};

struct wifi_ap_record_t {
    std::uint8_t ssid[33]{};
    std::int8_t rssi{-55};
    wifi_auth_mode_t authmode{WIFI_AUTH_WPA2_PSK};
};

inline wifi_config_t& mock_wifi_sta_config() {
    static wifi_config_t config{};
    return config;
}

inline esp_err_t esp_wifi_init(const wifi_init_config_t*) { return ESP_OK; }
inline esp_err_t esp_wifi_deinit() { return ESP_OK; }
inline esp_err_t esp_wifi_set_mode(wifi_mode_t) { return ESP_OK; }
inline esp_err_t esp_wifi_start() { return ESP_OK; }
inline esp_err_t esp_wifi_stop() { return ESP_OK; }
inline esp_err_t esp_wifi_connect() { return ESP_OK; }
inline esp_err_t esp_wifi_disconnect() { return ESP_OK; }

inline esp_err_t esp_wifi_set_config(wifi_interface_t interface, const wifi_config_t* config) {
    if (config == nullptr) return ESP_ERR_INVALID_ARG;
    if (interface == WIFI_IF_STA) mock_wifi_sta_config() = *config;
    return ESP_OK;
}

inline esp_err_t esp_wifi_get_config(wifi_interface_t interface, wifi_config_t* config) {
    if (config == nullptr) return ESP_ERR_INVALID_ARG;
    if (interface == WIFI_IF_STA) *config = mock_wifi_sta_config();
    return ESP_OK;
}

inline esp_err_t esp_wifi_get_mac(wifi_interface_t, std::uint8_t mac[6]) {
    if (mac == nullptr) return ESP_ERR_INVALID_ARG;
    const std::uint8_t value[6] = {0xAC, 0xA7, 0x04, 0x1D, 0xA7, 0x11};
    std::memcpy(mac, value, sizeof(value));
    return ESP_OK;
}

inline esp_err_t esp_wifi_sta_get_ap_info(wifi_ap_record_t*) { return ESP_FAIL; }
inline esp_err_t esp_wifi_scan_start(const wifi_scan_config_t*, bool) { return ESP_OK; }
inline esp_err_t esp_wifi_scan_get_ap_num(std::uint16_t* count) {
    if (count != nullptr) *count = 0;
    return ESP_OK;
}
inline esp_err_t esp_wifi_scan_get_ap_records(std::uint16_t* count, wifi_ap_record_t*) {
    if (count != nullptr) *count = 0;
    return ESP_OK;
}
