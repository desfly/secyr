#include "hg_setup_ap_guard.hpp"
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
constexpr TickType_t kDisableRetryDelay = pdMS_TO_TICKS(250);
constexpr unsigned kDisableRetryCount = 8;
constexpr unsigned kTaskStackBytes = 12288;
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

esp_err_t disable_setup_ap()
{
    wifi_mode_t mode = WIFI_MODE_NULL;
    const auto mode_error = esp_wifi_get_mode(&mode);
    if (mode_error == ESP_OK && mode == WIFI_MODE_STA) {
        ESP_LOGI(kTag, "Setup AP already disabled; Wi-Fi is STA-only");
        return ESP_OK;
    }

    const auto error = esp_wifi_set_mode(WIFI_MODE_STA);
    if (error == ESP_OK) {
        ESP_LOGI(kTag, "Open setup AP disabled; Wi-Fi is now STA-only");
    } else {
        ESP_LOGE(kTag, "Unable to disable setup AP: %s", esp_err_to_name(error));
    }
    return error;
}

esp_err_t disable_setup_ap_with_retries()
{
    esp_err_t last_error = ESP_FAIL;
    for (unsigned attempt = 0; attempt < kDisableRetryCount; ++attempt) {
        if (access_runtime::bootstrap_allowed()) return ESP_ERR_INVALID_STATE;
        last_error = disable_setup_ap();
        if (last_error == ESP_OK) return ESP_OK;
        vTaskDelay(kDisableRetryDelay);
    }
    return last_error;
}

void setup_ap_guard_task(void*)
{
    for (;;) {
        while (access_runtime::bootstrap_allowed()) {
            vTaskDelay(kPollDelay);
        }

        // The bootstrap HTTP response must have time to leave the socket before
        // dropping the SoftAP that carried that request. Re-check afterwards:
        // a failed Admin persist rolls bootstrap back to allowed=true.
        vTaskDelay(kPostBootstrapResponseDelay);
        if (access_runtime::bootstrap_allowed()) continue;

        const auto error = disable_setup_ap_with_retries();
        if (error != ESP_OK && error != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(kTag, "Setup AP remained enabled after Admin bootstrap: %s", esp_err_to_name(error));
        }
        vTaskDelete(nullptr);
    }
}

}  // namespace

esp_err_t start_setup_ap_guard()
{
    // Normal boot with a persisted Admin must be STA-only from this point on.
    // Wi-Fi is fully initialized before this function is called from app_main.
    if (!access_runtime::bootstrap_allowed()) {
        return disable_setup_ap();
    }

    if (!configure_setup_captive_portal()) {
        ESP_LOGW(kTag, "Setup captive portal advertisement unavailable; keeping bootstrap AP active");
    }

    const auto result = xTaskCreate(&setup_ap_guard_task,
                                    "hg_setup_ap_guard",
                                    kTaskStackBytes,
                                    nullptr,
                                    kTaskPriority,
                                    nullptr);
    if (result != pdPASS) {
        ESP_LOGE(kTag, "Unable to start setup AP guard task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(kTag, "Setup AP guard task started after Wi-Fi initialization");
    return ESP_OK;
}

}  // namespace homeguard::idf
