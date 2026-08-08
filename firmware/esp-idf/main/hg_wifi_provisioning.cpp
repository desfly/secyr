#include "hg_wifi_provisioning.hpp"

#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace homeguard::idf {
namespace {
constexpr const char* kTag = "hg_wifi";
}

esp_err_t WifiProvisioningRuntime::start(bool provisioning_required)
{
    ESP_LOGI(kTag, "=== HomeGuard-S3 Build-0057 WiFi Diagnostics ===");
    ESP_LOGI(kTag, "WiFi hardware: ESP32-S3");
    ESP_LOGI(kTag, "Provisioning required: %s", provisioning_required ? "YES" : "NO");

    if (!provisioning_required) {
        ESP_LOGI(kTag, "SoftAP start: SKIPPED");
        ESP_LOGI(kTag, "===============================================");
        return ESP_OK;
    }

    auto* ap_netif = esp_netif_create_default_wifi_ap();
    if (ap_netif == nullptr) {
        ESP_LOGE(kTag, "WiFi radio init: FAILED (netif)");
        return ESP_FAIL;
    }

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    auto error = esp_wifi_init(&init);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "WiFi radio init: FAILED (%s)", esp_err_to_name(error));
        return error;
    }
    ESP_LOGI(kTag, "WiFi radio init: OK");

    std::uint8_t mac[6]{};
    error = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "WiFi MAC read: FAILED (%s)", esp_err_to_name(error));
        return error;
    }

    std::snprintf(ssid_.data(), ssid_.size(), "HomeGuard-S3-%02X%02X", mac[4], mac[5]);
    ESP_LOGI(kTag, "WiFi MAC: %02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    wifi_config_t config{};
    std::strncpy(reinterpret_cast<char*>(config.ap.ssid), ssid_.data(), sizeof(config.ap.ssid) - 1);
    config.ap.ssid_len = static_cast<std::uint8_t>(std::strlen(ssid_.data()));
    config.ap.channel = 1;
    config.ap.max_connection = 4;
    config.ap.authmode = WIFI_AUTH_OPEN;

    if ((error = esp_wifi_set_mode(WIFI_MODE_AP)) != ESP_OK ||
        (error = esp_wifi_set_config(WIFI_IF_AP, &config)) != ESP_OK ||
        (error = esp_wifi_start()) != ESP_OK) {
        ESP_LOGE(kTag, "SoftAP start: FAILED (%s)", esp_err_to_name(error));
        return error;
    }

    esp_netif_ip_info_t ip{};
    error = esp_netif_get_ip_info(ap_netif, &ip);
    if (error == ESP_OK) {
        std::snprintf(ip_address_.data(), ip_address_.size(), IPSTR, IP2STR(&ip.ip));
    } else {
        std::strncpy(ip_address_.data(), "192.168.4.1", ip_address_.size() - 1);
    }

    started_ = true;
    ESP_LOGI(kTag, "SoftAP start: OK");
    ESP_LOGI(kTag, "SoftAP SSID: %s", ssid_.data());
    ESP_LOGI(kTag, "SoftAP IP: %s", ip_address_.data());
    ESP_LOGI(kTag, "SoftAP channel: 1");
    ESP_LOGI(kTag, "BLE provisioning: NOT_YET_ENABLED");
    ESP_LOGI(kTag, "Physical outputs: FAIL-CLOSED until commissioning is verified");
    ESP_LOGI(kTag, "===============================================");
    return ESP_OK;
}

}  // namespace homeguard::idf
