#include "setup_ap.hpp"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include <algorithm>
#include <cstring>

namespace { constexpr char tag[] = "hg_setup_ap"; }

bool SetupAp::begin(const SetupApConfig& config) {
    if (active_) return true;
    if (config.ssid.empty() || config.ssid.size() > 32U || config.password.size() < 12U || config.password.size() > 63U) return false;
    if (config.channel < 1U || config.channel > 13U || config.max_clients == 0U || config.max_clients > 4U) return false;

    ESP_ERROR_CHECK(esp_netif_init());
    const esp_err_t loop = esp_event_loop_create_default();
    if (loop != ESP_OK && loop != ESP_ERR_INVALID_STATE) return false;
    if (!esp_netif_create_default_wifi_ap()) return false;

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&init) != ESP_OK) return false;
    if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK) return false;

    wifi_config_t wifi{};
    const size_t ssid_length = std::min(config.ssid.size(), sizeof(wifi.ap.ssid));
    const size_t password_length = std::min(config.password.size(), sizeof(wifi.ap.password));
    std::memcpy(wifi.ap.ssid, config.ssid.data(), ssid_length);
    std::memcpy(wifi.ap.password, config.password.data(), password_length);
    wifi.ap.ssid_len = static_cast<uint8_t>(ssid_length);
    wifi.ap.channel = config.channel;
    wifi.ap.max_connection = config.max_clients;
    wifi.ap.authmode = WIFI_AUTH_WPA2_PSK;
    wifi.ap.pmf_cfg.required = true;

    if (esp_wifi_set_config(WIFI_IF_AP, &wifi) != ESP_OK) return false;
    if (esp_wifi_start() != ESP_OK) return false;
    active_ = true;
    ESP_LOGI(tag, "secure setup AP started; credentials intentionally not logged");
    return true;
}

void SetupAp::stop() {
    if (!active_) return;
    esp_wifi_stop();
    active_ = false;
    ESP_LOGI(tag, "setup AP stopped");
}
