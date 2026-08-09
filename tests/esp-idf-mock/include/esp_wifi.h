#pragma once
#include "esp_err.h"
#include <cstdint>

struct wifi_init_config_t {};
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}

enum wifi_mode_t { WIFI_MODE_STA = 1, WIFI_MODE_AP = 2, WIFI_MODE_APSTA = 3 };
enum wifi_interface_t { WIFI_IF_STA = 0, WIFI_IF_AP = 1 };
enum wifi_auth_mode_t { WIFI_AUTH_OPEN = 0, WIFI_AUTH_WPA2_PSK = 3 };

constexpr esp_err_t ESP_ERR_WIFI_NOT_CONNECT = 0x3006;

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

struct wifi_sta_threshold_t { wifi_auth_mode_t authmode{WIFI_AUTH_OPEN}; };
struct wifi_sta_config_t {
    std::uint8_t ssid[32]{};
    std::uint8_t password[64]{};
    wifi_sta_threshold_t threshold{};
};

struct wifi_config_t { wifi_ap_config_t ap{}; wifi_sta_config_t sta{}; };

struct wifi_scan_config_t {
    const std::uint8_t* ssid{};
    const std::uint8_t* bssid{};
    std::uint8_t channel{};
    bool show_hidden{};
};

struct wifi_ap_record_t {
    std::uint8_t bssid[6]{};
    std::uint8_t ssid[33]{};
    std::uint8_t primary{};
    std::int8_t rssi{};
};

inline esp_err_t esp_wifi_init(const wifi_init_config_t*) { return ESP_OK; }
inline esp_err_t esp_wifi_set_mode(wifi_mode_t) { return ESP_OK; }
inline esp_err_t esp_wifi_set_config(wifi_interface_t, const wifi_config_t*) { return ESP_OK; }
inline esp_err_t esp_wifi_get_config(wifi_interface_t, wifi_config_t* config) {
    if (config != nullptr) *config = wifi_config_t{};
    return ESP_OK;
}
inline esp_err_t esp_wifi_start() { return ESP_OK; }
inline esp_err_t esp_wifi_stop() { return ESP_OK; }
inline esp_err_t esp_wifi_connect() { return ESP_OK; }
inline esp_err_t esp_wifi_disconnect() { return ESP_OK; }
inline esp_err_t esp_wifi_deinit() { return ESP_OK; }
inline esp_err_t esp_wifi_scan_start(const wifi_scan_config_t*, bool) { return ESP_OK; }
inline esp_err_t esp_wifi_scan_get_ap_num(std::uint16_t* count) {
    if (count != nullptr) *count = 0;
    return ESP_OK;
}
inline esp_err_t esp_wifi_scan_get_ap_records(std::uint16_t* count, wifi_ap_record_t*) {
    if (count != nullptr) *count = 0;
    return ESP_OK;
}
