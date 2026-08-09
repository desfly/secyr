#include "esp_crt_bundle.h"
#include "esp_mac.h"
#include "mqtt_client.h"

#include <cstring>

extern "C" void app_main();

extern "C" esp_err_t esp_read_mac(uint8_t* mac, esp_mac_type_t type)
{
    if (mac == nullptr) return ESP_ERR_INVALID_ARG;
    const uint8_t base[6] = {0xAC, 0xA7, 0x04, 0x1D, 0xA7, 0x10};
    std::memcpy(mac, base, sizeof(base));
    if (type == ESP_MAC_WIFI_SOFTAP) mac[5] = static_cast<uint8_t>(mac[5] + 1U);
    return ESP_OK;
}

extern "C" esp_err_t esp_crt_bundle_attach(void*)
{
    return ESP_OK;
}

struct esp_mqtt_client {
    esp_event_handler_t handler{};
    void* handler_arg{};
};

extern "C" esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t*)
{
    static esp_mqtt_client client{};
    return &client;
}

extern "C" esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t client,
                                                       esp_mqtt_event_id_t,
                                                       esp_event_handler_t event_handler,
                                                       void* event_handler_arg)
{
    if (client == nullptr) return ESP_ERR_INVALID_ARG;
    client->handler = event_handler;
    client->handler_arg = event_handler_arg;
    return ESP_OK;
}

extern "C" esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t client)
{
    return client == nullptr ? ESP_ERR_INVALID_ARG : ESP_OK;
}

extern "C" esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t client)
{
    return client == nullptr ? ESP_ERR_INVALID_ARG : ESP_OK;
}

extern "C" esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t client)
{
    return client == nullptr ? ESP_ERR_INVALID_ARG : ESP_OK;
}

extern "C" int esp_mqtt_client_publish(esp_mqtt_client_handle_t client,
                                         const char*,
                                         const char*,
                                         int,
                                         int,
                                         int)
{
    return client == nullptr ? -1 : 1;
}

extern "C" int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client,
                                           const char*,
                                           int)
{
    return client == nullptr ? -1 : 1;
}

int main()
{
    app_main();
    return 0;
}
