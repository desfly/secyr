#pragma once

#include "esp_err.h"
#include "esp_event.h"
#include <cstdint>

struct esp_mqtt_client;
using esp_mqtt_client_handle_t = esp_mqtt_client*;

enum esp_mqtt_event_id_t {
    MQTT_EVENT_ANY = -1,
    MQTT_EVENT_ERROR = 0,
    MQTT_EVENT_CONNECTED = 1,
    MQTT_EVENT_DISCONNECTED = 2,
    MQTT_EVENT_SUBSCRIBED = 3,
    MQTT_EVENT_UNSUBSCRIBED = 4,
    MQTT_EVENT_PUBLISHED = 5,
    MQTT_EVENT_DATA = 6,
};

struct esp_mqtt_event_t {
    esp_mqtt_event_id_t event_id{MQTT_EVENT_ANY};
    esp_mqtt_client_handle_t client{};
    const char* topic{};
    int topic_len{};
    const char* data{};
    int data_len{};
};
using esp_mqtt_event_handle_t = esp_mqtt_event_t*;

struct esp_mqtt_client_config_t {
    struct broker_t {
        struct address_t { const char* uri{}; } address;
        struct verification_t { esp_err_t (*crt_bundle_attach)(void*){}; } verification;
    } broker;
    struct credentials_t {
        const char* username{};
        const char* client_id{};
        struct authentication_t { const char* password{}; } authentication;
    } credentials;
    struct session_t {
        struct last_will_t {
            const char* topic{};
            const char* msg{};
            int msg_len{};
            int qos{};
            int retain{};
        } last_will;
        int keepalive{};
    } session;
    struct network_t {
        int reconnect_timeout_ms{};
        int timeout_ms{};
    } network;
};

inline esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t*) { return reinterpret_cast<esp_mqtt_client_handle_t>(1); }
inline esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t, int, void(*)(void*,esp_event_base_t,std::int32_t,void*), void*) { return ESP_OK; }
inline esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t) { return ESP_OK; }
inline esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t) { return ESP_OK; }
inline esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t) { return ESP_OK; }
inline int esp_mqtt_client_publish(esp_mqtt_client_handle_t, const char*, const char*, int, int, int) { return 1; }
inline int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t, const char*, int) { return 1; }
