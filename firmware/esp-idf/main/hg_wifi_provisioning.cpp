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

void WifiProvisioningRuntime::network_event_handler(void* arg,
                                                    esp_event_base_t event_base,
                                                    std::int32_t event_id,
                                                    void* event_data)
{
    auto* self = static_cast<WifiProvisioningRuntime*>(arg);
    if (self == nullptr) return;

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP && event_data != nullptr) {
        const auto* event = static_cast<const ip_event_got_ip_t*>(event_data);
        std::snprintf(self->station_ip_address_.data(),
                      self->station_ip_address_.size(),
                      IPSTR,
                      IP2STR(&event->ip_info.ip));
        self->station_connecting_ = false;
        self->station_connected_ = true;
        ESP_LOGI(kTag, "STA connected; IP: %s", self->station_ip_address_.data());
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        self->station_connecting_ = false;
        self->station_connected_ = false;
        self->station_ip_address_[0] = '\0';
        ESP_LOGW(kTag, "STA disconnected");
    }
}

esp_err_t WifiProvisioningRuntime::start(bool provisioning_required)
{
    ESP_LOGI(kTag, "=== HomeGuard-S3 Build-0058 WiFi Diagnostics ===");
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
    (void)esp_netif_create_default_wifi_sta();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    auto error = esp_wifi_init(&init);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "WiFi radio init: FAILED (%s)", esp_err_to_name(error));
        return error;
    }
    ESP_LOGI(kTag, "WiFi radio init: OK");

    error = esp_event_handler_register(IP_EVENT,
                                       IP_EVENT_STA_GOT_IP,
                                       &WifiProvisioningRuntime::network_event_handler,
                                       this);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "STA GOT_IP handler registration failed: %s", esp_err_to_name(error));
        return error;
    }
    error = esp_event_handler_register(WIFI_EVENT,
                                       WIFI_EVENT_STA_DISCONNECTED,
                                       &WifiProvisioningRuntime::network_event_handler,
                                       this);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "STA disconnect handler registration failed: %s", esp_err_to_name(error));
        return error;
    }

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

    if ((error = esp_wifi_set_mode(WIFI_MODE_APSTA)) != ESP_OK ||
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
    ESP_LOGI(kTag, "STA provisioning: READY");
    ESP_LOGI(kTag, "Physical outputs: FAIL-CLOSED until commissioning is verified");
    ESP_LOGI(kTag, "===============================================");
    return ESP_OK;
}

esp_err_t WifiProvisioningRuntime::connect_station(const char* ssid, const char* password)
{
    if (ssid == nullptr || ssid[0] == '\0' || password == nullptr) return ESP_ERR_INVALID_ARG;

    if (station_connected_ || station_connecting_) {
        const auto disconnect_error = esp_wifi_disconnect();
        if (disconnect_error != ESP_OK && disconnect_error != ESP_ERR_WIFI_NOT_CONNECT) {
            ESP_LOGW(kTag, "STA disconnect before reprovision failed: %s",
                     esp_err_to_name(disconnect_error));
            return disconnect_error;
        }
    }

    station_connecting_ = false;
    station_connected_ = false;
    station_ip_address_[0] = '\0';

    wifi_config_t config{};
    std::strncpy(reinterpret_cast<char*>(config.sta.ssid), ssid, sizeof(config.sta.ssid) - 1);
    std::strncpy(reinterpret_cast<char*>(config.sta.password), password, sizeof(config.sta.password) - 1);
    config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    auto error = esp_wifi_set_config(WIFI_IF_STA, &config);
    if (error != ESP_OK) return error;

    station_connecting_ = true;
    error = esp_wifi_connect();
    if (error != ESP_OK) {
        station_connecting_ = false;
        return error;
    }

    ESP_LOGI(kTag, "STA connect requested for SSID: %s", ssid);
    return ESP_OK;
}

}  // namespace homeguard::idf
