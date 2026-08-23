#include "hg_access_runtime.hpp"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

namespace homeguard::idf {
namespace {

constexpr const char* kTag = "hg_setup_ap";
constexpr TickType_t kPollDelay = pdMS_TO_TICKS(100);
constexpr TickType_t kPostBootstrapResponseDelay = pdMS_TO_TICKS(750);
constexpr unsigned kTaskStackBytes = 8192;
constexpr unsigned kTaskPriority = 3;
static char kCaptivePortalUri[] = "http://192.168.4.1";

bool configure_setup_captive_portal()
{
#ifdef CONFIG_ESP_ENABLE_DHCP_CAPTIVEPORTAL
    auto* ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    if (ap_netif == nullptr) return false;

    const auto stop_error = esp_netif_dhcps_stop(ap_netif);
    if (stop_error != ESP_OK && stop_error != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) {
        return false;
    }

    const auto option_error = esp_netif_dhcps_option(ap_netif,
                                                     ESP_NETIF_OP_SET,
                                                     ESP_NETIF_CAPTIVEPORTAL_URI,
                                                     kCaptivePortalUri,
                                                     std::strlen(kCaptivePortalUri));
    if (option_error != ESP_OK) {
        (void)esp_netif_dhcps_start(ap_netif);
        return false;
    }

    const auto start_error = esp_netif_dhcps_start(ap_netif);
    if (start_error != ESP_OK && start_error != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
        return false;
    }

    ESP_LOGI(kTag, "Android/iOS captive portal advertised at %s", kCaptivePortalUri);
#endif
    return true;
}

void setup_ap_guard_task(void*)
{
    ESP_LOGI(kTag, "Setup AP guard task started");

    bool bootstrap_seen = false;
    bool response_delay_applied = false;
    bool captive_portal_configured = false;

    for (;;) {
        if (access_runtime::bootstrap_allowed()) {
            bootstrap_seen = true;
            response_delay_applied = false;
            if (!captive_portal_configured) {
                captive_portal_configured = configure_setup_captive_portal();
            }
            vTaskDelay(kPollDelay);
            continue;
        }

        // When the first Admin has just been persisted, leave enough time for
        // the bootstrap HTTP response to reach the browser before the AP drops.
        // On later boots with an existing Admin there is no response to protect,
        // so the guard immediately starts trying to force STA-only mode.
        if (bootstrap_seen && !response_delay_applied) {
            response_delay_applied = true;
            vTaskDelay(kPostBootstrapResponseDelay);
        }

        // Before esp_wifi_init()/esp_wifi_start() this returns an error. Retry
        // until Wi-Fi is ready. Once ready, setting STA mode removes the open
        // 192.168.4.1 setup AP while preserving the configured station link.
        const auto set_mode_error = esp_wifi_set_mode(WIFI_MODE_STA);
        if (set_mode_error == ESP_OK) {
            ESP_LOGI(kTag, "Open setup AP disabled; Wi-Fi is now STA-only");
            break;
        }

        vTaskDelay(kPollDelay);
    }

    vTaskDelete(nullptr);
}

class SetupApGuardStarter {
public:
    SetupApGuardStarter()
    {
        if (xTaskCreate(&setup_ap_guard_task,
                        "hg_setup_ap_guard",
                        kTaskStackBytes,
                        nullptr,
                        kTaskPriority,
                        nullptr) != pdPASS) {
            ESP_LOGE(kTag, "Unable to start setup AP guard task");
        }
    }
};

SetupApGuardStarter g_setup_ap_guard_starter;

}  // namespace
}  // namespace homeguard::idf
