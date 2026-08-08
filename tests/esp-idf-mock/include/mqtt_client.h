#pragma once

#include "esp_err.h"
#include "esp_event.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_mqtt_client* esp_mqtt_client_handle_t;

typedef enum {
    MQTT_EVENT_ANY = -1,
    MQTT_EVENT_ERROR = 0,
    MQTT_EVENT_CONNECTED,
    MQTT_EVENT_DISCONNECTED,
    MQTT_EVENT_SUBSCRIBED,
    MQTT_EVENT_UNSUBSCRIBED,
    MQTT_EVENT_PUBLISHED,
    MQTT_EVENT_DATA,
    MQTT_EVENT_BEFORE_CONNECT,
    MQTT_EVENT_DELETED,
    MQTT_USER_EVENT
} esp_mqtt_event_id_t;

typedef struct esp_mqtt_event {
    esp_mqtt_event_id_t event_id;
    esp_mqtt_client_handle_t client;
    void* user_context;
    const char* data;
    int data_len;
    const char* topic;
    int topic_len;
} esp_mqtt_event_t;

typedef esp_mqtt_event_t* esp_mqtt_event_handle_t;

typedef struct {
    struct {
        struct { const char* uri; } address;
        struct { esp_err_t (*crt_bundle_attach)(void*); } verification;
    } broker;
    struct {
        const char* username;
        const char* client_id;
        struct { const char* password; } authentication;
    } credentials;
    struct {
        struct {
            const char* topic;
            const char* msg;
            int msg_len;
            int qos;
            int retain;
        } last_will;
        int keepalive;
    } session;
    struct {
        int reconnect_timeout_ms;
        int timeout_ms;
    } network;
} esp_mqtt_client_config_t;

esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t* config);
esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client, esp_mqtt_event_id_t event, esp_event_handler_t event_handler, void* event_handler_arg);
esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client);
esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t client);
esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t client);
int esp_mqtt_client_publish(esp_mqtt_client_handle_t client, const char* topic, const char* data, int len, int qos, int retain);
int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client, const char* topic, int qos);

#ifdef __cplusplus
}
#endif
