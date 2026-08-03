#!/usr/bin/env python3
"""Controlled ESP-IDF API mocks used only by local compile/link gates."""
MOCKS = {
    'esp_log.h': r'''#pragma once
#include <cstdint>
using esp_err_t=int;
inline constexpr esp_err_t ESP_OK=0;
inline constexpr esp_err_t ESP_FAIL=-1;
#define ESP_LOGI(tag,fmt,...) ((void)0)
#define ESP_LOGW(tag,fmt,...) ((void)0)
#define ESP_LOGE(tag,fmt,...) ((void)0)
#define ESP_ERROR_CHECK(x) do { (void)(x); } while(0)
''',
    'nvs_flash.h': r'''#pragma once
#include "esp_log.h"
inline constexpr esp_err_t ESP_ERR_NVS_NO_FREE_PAGES=1;
inline constexpr esp_err_t ESP_ERR_NVS_NEW_VERSION_FOUND=2;
inline esp_err_t nvs_flash_init(){return ESP_OK;}
inline esp_err_t nvs_flash_erase(){return ESP_OK;}
''',
    'nvs.h': r'''#pragma once
#include "esp_log.h"
#include <cstddef>
#include <cstdint>
using nvs_handle_t=uint32_t;
inline constexpr int NVS_READONLY=0;
inline constexpr int NVS_READWRITE=1;
inline esp_err_t nvs_open(const char*,int,nvs_handle_t* h){*h=1;return ESP_OK;}
inline void nvs_close(nvs_handle_t){}
inline esp_err_t nvs_get_str(nvs_handle_t,const char*,char*,size_t*){return ESP_OK;}
inline esp_err_t nvs_set_str(nvs_handle_t,const char*,const char*){return ESP_OK;}
inline esp_err_t nvs_get_blob(nvs_handle_t,const char*,void*,size_t*){return ESP_OK;}
inline esp_err_t nvs_set_blob(nvs_handle_t,const char*,const void*,size_t){return ESP_OK;}
inline esp_err_t nvs_get_u8(nvs_handle_t,const char*,uint8_t* v){*v=1;return ESP_OK;}
inline esp_err_t nvs_set_u8(nvs_handle_t,const char*,uint8_t){return ESP_OK;}
inline esp_err_t nvs_erase_all(nvs_handle_t){return ESP_OK;}
inline esp_err_t nvs_commit(nvs_handle_t){return ESP_OK;}
''',
    'esp_mac.h': r'''#pragma once
#include "esp_log.h"
inline constexpr int ESP_MAC_WIFI_STA=0;
inline esp_err_t esp_read_mac(unsigned char*,int){return ESP_OK;}
''',
    'esp_event.h': r'''#pragma once
#include "esp_log.h"
#include <cstdint>
using esp_event_base_t=const char*;
using esp_event_handler_instance_t=void*;
using esp_event_handler_t=void(*)(void*,esp_event_base_t,int32_t,void*);
inline constexpr esp_err_t ESP_ERR_INVALID_STATE=3;
inline constexpr int32_t ESP_EVENT_ANY_ID=-1;
inline constexpr char WIFI_EVENT_NAME[]="WIFI_EVENT";
inline constexpr char IP_EVENT_NAME[]="IP_EVENT";
inline constexpr esp_event_base_t WIFI_EVENT=WIFI_EVENT_NAME;
inline constexpr esp_event_base_t IP_EVENT=IP_EVENT_NAME;
inline constexpr int32_t WIFI_EVENT_STA_START=1;
inline constexpr int32_t WIFI_EVENT_STA_DISCONNECTED=2;
inline constexpr int32_t IP_EVENT_STA_GOT_IP=3;
inline esp_err_t esp_event_loop_create_default(){return ESP_OK;}
inline esp_err_t esp_event_handler_instance_register(esp_event_base_t,int32_t,esp_event_handler_t,void*,esp_event_handler_instance_t* out){*out=reinterpret_cast<void*>(1);return ESP_OK;}
inline esp_err_t esp_event_handler_instance_unregister(esp_event_base_t,int32_t,esp_event_handler_instance_t){return ESP_OK;}
''',
    'esp_netif.h': r'''#pragma once
#include "esp_log.h"
#include <cstdint>
struct esp_netif_t{};
struct esp_ip4_addr_t{uint32_t addr{};};
struct esp_netif_ip_info_t{esp_ip4_addr_t ip{};};
struct ip_event_got_ip_t{esp_netif_ip_info_t ip_info{};};
inline esp_err_t esp_netif_init(){return ESP_OK;}
inline esp_netif_t* esp_netif_create_default_wifi_ap(){static esp_netif_t n;return &n;}
inline esp_netif_t* esp_netif_create_default_wifi_sta(){static esp_netif_t n;return &n;}
inline void esp_netif_destroy_default_wifi(esp_netif_t*){}
''',
    'esp_wifi.h': r'''#pragma once
#include "esp_log.h"
#include <cstdint>
struct wifi_init_config_t{};
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}
inline constexpr int WIFI_MODE_STA=1;
inline constexpr int WIFI_MODE_APSTA=2;
inline constexpr int WIFI_IF_STA=0;
inline constexpr int WIFI_IF_AP=1;
inline constexpr int WIFI_AUTH_WPA2_PSK=3;
struct wifi_pmf_config_t{bool capable{};bool required{};};
struct wifi_threshold_t{int authmode{};};
struct wifi_ap_config_t{uint8_t ssid[32]{};uint8_t password[64]{};uint8_t ssid_len{};uint8_t channel{};uint8_t max_connection{};int authmode{};wifi_pmf_config_t pmf_cfg{};};
struct wifi_sta_config_t{uint8_t ssid[32]{};uint8_t password[64]{};wifi_threshold_t threshold{};wifi_pmf_config_t pmf_cfg{};};
union wifi_config_t{wifi_ap_config_t ap;wifi_sta_config_t sta;wifi_config_t():ap{} {}};
inline esp_err_t esp_wifi_init(const wifi_init_config_t*){return ESP_OK;}
inline esp_err_t esp_wifi_set_mode(int){return ESP_OK;}
inline esp_err_t esp_wifi_set_config(int,const wifi_config_t*){return ESP_OK;}
inline esp_err_t esp_wifi_start(){return ESP_OK;}
inline esp_err_t esp_wifi_stop(){return ESP_OK;}
inline esp_err_t esp_wifi_connect(){return ESP_OK;}
inline esp_err_t esp_wifi_disconnect(){return ESP_OK;}
inline esp_err_t esp_wifi_deinit(){return ESP_OK;}
''',
    'esp_random.h': r'''#pragma once
#include <cstdint>
inline uint32_t esp_random(){return 12345678U;}
''',
    'sdkconfig.h': r'''#pragma once
#define CONFIG_HOMEGUARD_API_PORT 443
#define CONFIG_HOMEGUARD_TELEMETRY_INTERVAL_MS 1000
#define CONFIG_HOMEGUARD_LOCAL_TLS 1
#define CONFIG_HOMEGUARD_CLOUD_ENABLED 1
#define CONFIG_HOMEGUARD_CLOUD_BROKER_URI "mqtts://example.invalid:8883"
#define CONFIG_HOMEGUARD_CLOUD_ACCESS_TOKEN "token"
#define CONFIG_HOMEGUARD_TASK_WATCHDOG_SECONDS 8
#define CONFIG_HOMEGUARD_SETUP_AP_ENABLED 1
#define CONFIG_HOMEGUARD_SETUP_AP_TIMEOUT_SECONDS 900
#define CONFIG_HOMEGUARD_PAIRING_CODE_TTL_SECONDS 600
#define CONFIG_HOMEGUARD_PAIRING_MAX_ATTEMPTS 5
#define CONFIG_HOMEGUARD_SETUP_AP_CHANNEL 6
#define CONFIG_HOMEGUARD_SETUP_HTTPS_PORT 8443
#define CONFIG_HOMEGUARD_SETUP_CERT_SHA256 "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
#define CONFIG_HOMEGUARD_REQUIRE_NVS_ENCRYPTION 1
#define CONFIG_HOMEGUARD_WIFI_CONNECT_TIMEOUT_SECONDS 30
#define CONFIG_HOMEGUARD_WIFI_RETRY_SECONDS 10
#define CONFIG_HOMEGUARD_I2C_SDA_GPIO -1
#define CONFIG_HOMEGUARD_I2C_SCL_GPIO -1
#define CONFIG_HOMEGUARD_W5500_MOSI_GPIO -1
#define CONFIG_HOMEGUARD_W5500_MISO_GPIO -1
#define CONFIG_HOMEGUARD_W5500_SCLK_GPIO -1
#define CONFIG_HOMEGUARD_W5500_CS_GPIO -1
#define CONFIG_HOMEGUARD_W5500_INT_GPIO -1
#define CONFIG_HOMEGUARD_W5500_RST_GPIO -1
#define CONFIG_HOMEGUARD_SERVICE_BUTTON_GPIO 21
#define CONFIG_HOMEGUARD_SERVICE_BUTTON_ACTIVE_LOW 1
#define CONFIG_HOMEGUARD_SERVICE_BUTTON_DEBOUNCE_MS 40
#define CONFIG_HOMEGUARD_SERVICE_HOLD_MS 3000
#define CONFIG_HOMEGUARD_FACTORY_RESET_HOLD_MS 10000
#define CONFIG_NVS_ENCRYPTION 1
''',
    'esp_task_wdt.h': r'''#pragma once
#include "esp_log.h"
#include <cstdint>
inline constexpr esp_err_t ESP_ERR_INVALID_STATE=3;
inline constexpr esp_err_t ESP_ERR_NOT_FOUND=4;
struct esp_task_wdt_config_t { uint32_t timeout_ms{}; uint32_t idle_core_mask{}; bool trigger_panic{}; };
inline esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t*){return ESP_OK;}
inline esp_err_t esp_task_wdt_add(void*){return ESP_OK;}
inline esp_err_t esp_task_wdt_reset(){return ESP_OK;}
inline esp_err_t esp_task_wdt_delete(void*){return ESP_OK;}
''',
    'esp_crt_bundle.h': r'''#pragma once
inline void esp_crt_bundle_attach(){}
''',
    'mqtt_client.h': r'''#pragma once
#include "esp_log.h"
#include <cstdint>
struct esp_mqtt_client{};
using esp_mqtt_client_handle_t=esp_mqtt_client*;
struct esp_mqtt_event_t { const char* topic{}; int topic_len{}; const char* data{}; int data_len{}; int total_data_len{}; int current_data_offset{}; };
using esp_mqtt_event_handle_t=esp_mqtt_event_t*;
inline constexpr int MQTT_EVENT_ANY=-1;
inline constexpr int MQTT_EVENT_CONNECTED=1;
inline constexpr int MQTT_EVENT_DISCONNECTED=2;
inline constexpr int MQTT_EVENT_DATA=3;
struct esp_mqtt_client_config_t {
 struct { struct { const char* uri{}; } address; struct { void (*crt_bundle_attach)(){}; } verification; } broker;
 struct { const char* client_id{}; const char* username{}; struct { const char* password{}; } authentication; } credentials;
 struct { int keepalive{}; bool disable_clean_session{}; } session;
 struct { int reconnect_timeout_ms{}; } network;
};
using mqtt_cb_t=void(*)(void*,const char*,int32_t,void*);
inline esp_mqtt_client_handle_t esp_mqtt_client_init(const esp_mqtt_client_config_t*){static esp_mqtt_client c;return &c;}
inline esp_err_t esp_mqtt_client_register_event(esp_mqtt_client_handle_t,int,mqtt_cb_t,void*){return ESP_OK;}
inline esp_err_t esp_mqtt_client_start(esp_mqtt_client_handle_t){return ESP_OK;}
inline esp_err_t esp_mqtt_client_stop(esp_mqtt_client_handle_t){return ESP_OK;}
inline esp_err_t esp_mqtt_client_destroy(esp_mqtt_client_handle_t){return ESP_OK;}
inline int esp_mqtt_client_publish(esp_mqtt_client_handle_t,const char*,const char*,int,int,int){return 1;}
inline int esp_mqtt_client_subscribe(esp_mqtt_client_handle_t,const char*,int){return 1;}
''',
    'mdns.h': r'''#pragma once
#include "esp_log.h"
#include <cstddef>
#include <cstdint>
struct mdns_txt_item_t { const char* key; const char* value; };
inline esp_err_t mdns_init(){return ESP_OK;}
inline void mdns_free(){}
inline esp_err_t mdns_hostname_set(const char*){return ESP_OK;}
inline esp_err_t mdns_instance_name_set(const char*){return ESP_OK;}
inline esp_err_t mdns_service_add(const char*,const char*,const char*,uint16_t,const mdns_txt_item_t*,size_t){return ESP_OK;}
inline esp_err_t mdns_service_remove(const char*,const char*){return ESP_OK;}
''',
    'cJSON.h': r'''#pragma once
#include <cstddef>
struct cJSON { char* valuestring{}; double valuedouble{}; int valueint{}; };
inline cJSON* cJSON_ParseWithLength(const char*,size_t){static char empty[]="";static cJSON value{empty};return &value;}
inline cJSON* cJSON_GetObjectItemCaseSensitive(cJSON*,const char*){static char empty[]="";static cJSON value{empty};return &value;}
inline int cJSON_IsString(const cJSON*){return 1;}
inline int cJSON_IsNumber(const cJSON*){return 1;}
inline void cJSON_Delete(cJSON*){}
''',
    'esp_https_server.h': r'''#pragma once
#include "esp_log.h"
#include <cstddef>
#include <cstdint>
#include <sys/types.h>
struct httpd_data{};
using httpd_handle_t=httpd_data*;
struct httpd_req { int content_len{}; void* user_ctx{}; int method{}; };
inline constexpr int HTTP_GET=0;
inline constexpr int HTTP_POST=1;
using httpd_uri_func_t=esp_err_t(*)(httpd_req_t*);
struct httpd_uri_t { const char* uri{}; int method{}; httpd_uri_func_t handler{}; void* user_ctx{}; bool is_websocket{}; };
struct httpd_config_t { uint16_t server_port{}; size_t max_uri_handlers{}; };
struct httpd_ssl_config_t { httpd_config_t httpd{}; const uint8_t* servercert{}; size_t servercert_len{}; const uint8_t* prvtkey_pem{}; size_t prvtkey_len{}; };
enum httpd_ws_type_t { HTTPD_WS_TYPE_CONTINUE, HTTPD_WS_TYPE_TEXT, HTTPD_WS_TYPE_BINARY, HTTPD_WS_TYPE_CLOSE, HTTPD_WS_TYPE_PING, HTTPD_WS_TYPE_PONG };
struct httpd_ws_frame_t { bool final{}; bool fragmented{}; httpd_ws_type_t type{HTTPD_WS_TYPE_TEXT}; uint8_t* payload{}; size_t len{}; };
#define HTTPD_SSL_CONFIG_DEFAULT() httpd_ssl_config_t{}
inline esp_err_t httpd_ssl_start(httpd_handle_t* out,const httpd_ssl_config_t*){static httpd_data d;*out=&d;return ESP_OK;}
inline esp_err_t httpd_ssl_stop(httpd_handle_t){return ESP_OK;}
inline esp_err_t httpd_register_uri_handler(httpd_handle_t,const httpd_uri_t*){return ESP_OK;}
inline esp_err_t httpd_resp_set_status(httpd_req_t*,const char*){return ESP_OK;}
inline esp_err_t httpd_resp_set_type(httpd_req_t*,const char*){return ESP_OK;}
inline esp_err_t httpd_resp_set_hdr(httpd_req_t*,const char*,const char*){return ESP_OK;}
inline esp_err_t httpd_resp_send(httpd_req_t*,const char*,ssize_t){return ESP_OK;}
inline int httpd_req_recv(httpd_req_t*,char*,int){return -1;}
inline size_t httpd_req_get_hdr_value_len(httpd_req_t*,const char*){return 0;}
inline esp_err_t httpd_req_get_hdr_value_str(httpd_req_t*,const char*,char*,size_t){return ESP_OK;}
inline int httpd_req_to_sockfd(httpd_req_t*){return 1;}
inline esp_err_t httpd_ws_recv_frame(httpd_req_t*,httpd_ws_frame_t*,size_t){return ESP_OK;}
inline esp_err_t httpd_ws_send_frame(httpd_req_t*,httpd_ws_frame_t*){return ESP_OK;}
inline esp_err_t httpd_ws_send_frame_async(httpd_handle_t,int,httpd_ws_frame_t*){return ESP_OK;}
using httpd_work_fn_t=void(*)(void*);
inline esp_err_t httpd_queue_work(httpd_handle_t,httpd_work_fn_t fn,void* arg){fn(arg);return ESP_OK;}
''',
    'esp_timer.h': r'''#pragma once
#include <cstdint>
inline int64_t esp_timer_get_time(){return 0;}
''',
    'freertos/FreeRTOS.h': r'''#pragma once
inline constexpr int pdPASS=1;
#define pdMS_TO_TICKS(ms) (ms)
''',
    'freertos/task.h': r'''#pragma once
#include "freertos/FreeRTOS.h"
using TaskFunction_t=void(*)(void*);
inline int xTaskCreate(TaskFunction_t,const char*,unsigned,void*,unsigned,void*){return pdPASS;}
inline void vTaskDelete(void*){}
inline void vTaskDelay(unsigned){}
''',
    'esp_system.h': r'''#pragma once
inline void esp_restart(){}
''',
    'driver/gpio.h': r'''#pragma once
#include "esp_log.h"
#include <cstdint>
using gpio_num_t=int;
inline constexpr int GPIO_MODE_INPUT=1;
inline constexpr int GPIO_PULLUP_ENABLE=1;
inline constexpr int GPIO_PULLUP_DISABLE=0;
inline constexpr int GPIO_PULLDOWN_ENABLE=1;
inline constexpr int GPIO_PULLDOWN_DISABLE=0;
inline constexpr int GPIO_INTR_DISABLE=0;
struct gpio_config_t{uint64_t pin_bit_mask{};int mode{};int pull_up_en{};int pull_down_en{};int intr_type{};};
inline esp_err_t gpio_config(const gpio_config_t*){return ESP_OK;}
inline int gpio_get_level(gpio_num_t){return 1;}
''',
    'lwip/ip4_addr.h': r'''#pragma once
#include "esp_netif.h"
#include <cstddef>
inline constexpr int IP4ADDR_STRLEN_MAX=16;
inline const char* esp_ip4addr_ntoa(const esp_ip4_addr_t*,char* buffer,size_t length){if(length>0){buffer[0]='0';if(length>1)buffer[1]='\0';}return buffer;}
''',
    'lwip/inet.h': r'''#pragma once
#include <arpa/inet.h>
''',
    'lwip/sockets.h': r'''#pragma once
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
''',
}
