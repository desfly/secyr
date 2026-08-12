#include "esp_crt_bundle.h"
#include "esp_mac.h"
#include "mqtt_client.h"

#include <cstdint>
#include <cstring>

extern "C" void app_main();

extern "C" {
extern const uint8_t _binary_index_html_start[] = {0};
extern const uint8_t _binary_index_html_end[] = {0};
extern const uint8_t _binary_app_css_start[] = {0};
extern const uint8_t _binary_app_css_end[] = {0};
extern const uint8_t _binary_app_js_start[] = {0};
extern const uint8_t _binary_app_js_end[] = {0};
extern const uint8_t _binary_access_session_js_start[] = {0};
extern const uint8_t _binary_access_session_js_end[] = {0};
extern const uint8_t _binary_cloud_status_js_start[] = {0};
extern const uint8_t _binary_cloud_status_js_end[] = {0};
extern const uint8_t _binary_config_ui_js_start[] = {0};
extern const uint8_t _binary_config_ui_js_end[] = {0};
extern const uint8_t _binary_lan_monitor_js_start[] = {0};
extern const uint8_t _binary_lan_monitor_js_end[] = {0};
extern const uint8_t _binary_bruce_jpg_start[] = {0};
extern const uint8_t _binary_bruce_jpg_end[] = {0};
}

struct esp_mqtt_client {};
static esp_mqtt_client g_mock_mqtt_client{};

extern "C" esp_err_t esp_read_mac(uint8_t* mac, esp_mac_type_t)
{
    if (mac == nullptr) return ESP_ERR_INVALID_ARG;
    const uint8_t fixed[6] = {0x02, 0x48, 0x47, 0x00, 0x00, 0x01};
    std::memcpy(mac, fixed, sizeof(fixed));
    return ESP_OK;
}

extern "C" esp_err_t esp_crt_bundle_attach(void*)
{
    return ESP_OK;
}

extern "C" esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t*)
{
    return &g_mock_mqtt_client;
}

extern "C" esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t, esp_mqtt_event_id_t, esp_event_handler_t, void*)
{
    return ESP_OK;
}

extern "C" esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t)
{
    return ESP_OK;
}

extern "C" esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t)
{
    return ESP_OK;
}

extern "C" esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t)
{
    return ESP_OK;
}

extern "C" int esp_mqtt_client_publish(esp_mqtt_client_handle_t, const char*, const char*, int, int, int)
{
    return 1;
}

extern "C" int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t, const char*, int)
{
    return 1;
}

int main()
{
    app_main();
    return 0;
}
