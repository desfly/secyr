#pragma once
#include "esp_err.h"
#include <cstdint>

using esp_event_base_t = const char*;
using esp_event_handler_t = void (*)(void*, esp_event_base_t, std::int32_t, void*);

constexpr esp_event_base_t ETH_EVENT = "ETH";
constexpr esp_event_base_t IP_EVENT = "IP";
constexpr esp_event_base_t WIFI_EVENT = "WIFI";

enum {
    ESP_EVENT_ANY_ID = -1,
    IP_EVENT_ETH_GOT_IP = 1,
    ETHERNET_EVENT_CONNECTED = 2,
    ETHERNET_EVENT_DISCONNECTED = 3,
    IP_EVENT_STA_GOT_IP = 4,
    WIFI_EVENT_STA_DISCONNECTED = 5,
    WIFI_EVENT_STA_CONNECTED = 6,
    WIFI_EVENT_STA_START = 7,
    WIFI_EVENT_STA_STOP = 8
};

inline esp_err_t esp_event_loop_create_default() { return ESP_OK; }
inline esp_err_t esp_event_handler_register(esp_event_base_t, int, esp_event_handler_t, void*) { return ESP_OK; }
