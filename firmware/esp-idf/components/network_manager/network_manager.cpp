#include "network_manager.hpp"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/ip4_addr.h"
#include <algorithm>
#include <cstring>
#include <utility>

namespace { constexpr char tag[] = "hg_network"; }

NetworkManager::~NetworkManager() { stop(); }

void NetworkManager::clear_password() {
    std::fill(password_.begin(), password_.end(), '\0');
    password_.clear();
}

void NetworkManager::rollback_init() {
    if (wifi_event_instance_) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID,
            reinterpret_cast<esp_event_handler_instance_t>(wifi_event_instance_));
        wifi_event_instance_ = nullptr;
    }
    if (ip_event_instance_) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP,
            reinterpret_cast<esp_event_handler_instance_t>(ip_event_instance_));
        ip_event_instance_ = nullptr;
    }
    if (wifi_initialized_) {
        esp_wifi_stop();
        esp_wifi_deinit();
        wifi_initialized_ = false;
    }
    if (sta_netif_) {
        esp_netif_destroy_default_wifi(static_cast<esp_netif_t*>(sta_netif_));
        sta_netif_ = nullptr;
    }
    initialized_ = false;
    connected_.store(false);
    {
        std::scoped_lock lock(state_mutex_);
        ipv4_.clear();
    }
    clear_password();
}

bool NetworkManager::begin(std::string ssid, std::string password) {
    if (initialized_) return true;
    if (ssid.empty() || ssid.size() > 32U || password.size() < 8U || password.size() > 64U) return false;
    ssid_ = std::move(ssid);
    password_ = std::move(password);

    const esp_err_t netif = esp_netif_init();
    if (netif != ESP_OK && netif != ESP_ERR_INVALID_STATE) { rollback_init(); return false; }
    const esp_err_t loop = esp_event_loop_create_default();
    if (loop != ESP_OK && loop != ESP_ERR_INVALID_STATE) { rollback_init(); return false; }
    sta_netif_ = esp_netif_create_default_wifi_sta();
    if (!sta_netif_) { rollback_init(); return false; }

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&init) != ESP_OK) { rollback_init(); return false; }
    wifi_initialized_ = true;
    if (esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &NetworkManager::event_entry,
                                            this, reinterpret_cast<esp_event_handler_instance_t*>(&wifi_event_instance_)) != ESP_OK) {
        rollback_init(); return false;
    }
    if (esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &NetworkManager::event_entry,
                                            this, reinterpret_cast<esp_event_handler_instance_t*>(&ip_event_instance_)) != ESP_OK) {
        rollback_init(); return false;
    }

    wifi_config_t wifi{};
    const size_t ssid_length = std::min(ssid_.size(), sizeof(wifi.sta.ssid));
    const size_t password_length = std::min(password_.size(), sizeof(wifi.sta.password));
    std::memcpy(wifi.sta.ssid, ssid_.data(), ssid_length);
    std::memcpy(wifi.sta.password, password_.data(), password_length);
    wifi.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi.sta.pmf_cfg.capable = true;
    wifi.sta.pmf_cfg.required = false;

    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK ||
        esp_wifi_set_config(WIFI_IF_STA, &wifi) != ESP_OK ||
        esp_wifi_start() != ESP_OK) {
        rollback_init();
        return false;
    }
    initialized_ = true;
    ESP_LOGI(tag, "Wi-Fi STA started; credentials intentionally not logged");
    return true;
}

bool NetworkManager::retry() {
    if (!initialized_) return false;
    connected_.store(false);
    return esp_wifi_connect() == ESP_OK;
}

void NetworkManager::stop() {
    if (!initialized_ && !wifi_initialized_ && !sta_netif_) { clear_password(); return; }
    esp_wifi_disconnect();
    rollback_init();
}

hg::LinkInputs NetworkManager::links() const { return {false, connected_.load(), false}; }

std::string NetworkManager::ipv4() const {
    std::scoped_lock lock(state_mutex_);
    return ipv4_;
}

void NetworkManager::event_entry(void* context, const char* base, const int32_t event_id, void* event_data) {
    static_cast<NetworkManager*>(context)->handle_event(base, event_id, event_data);
}

void NetworkManager::handle_event(const char* base, const int32_t event_id, void* event_data) {
    if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        connected_.store(false);
        {
            std::scoped_lock lock(state_mutex_);
            ipv4_.clear();
        }
        disconnect_count_.fetch_add(1);
        ESP_LOGW(tag, "Wi-Fi disconnected; retry delegated to startup supervisor");
    } else if (base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const auto* event = static_cast<const ip_event_got_ip_t*>(event_data);
        char address[IP4ADDR_STRLEN_MAX]{};
        esp_ip4addr_ntoa(&event->ip_info.ip, address, sizeof(address));
        {
            std::scoped_lock lock(state_mutex_);
            ipv4_ = address;
        }
        connected_.store(true);
        ESP_LOGI(tag, "Wi-Fi STA received IPv4 address");
    }
}
