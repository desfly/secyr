#include "hg_setup_ap_guard.hpp"
#include "hg_access_runtime.hpp"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_ip_addr.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstring>

namespace homeguard::idf {
namespace {

constexpr const char* kTag = "hg_setup_ap";
constexpr TickType_t kPollDelay = pdMS_TO_TICKS(100);
constexpr TickType_t kPostBootstrapResponseDelay = pdMS_TO_TICKS(750);
constexpr TickType_t kStaReadyPollDelay = pdMS_TO_TICKS(250);
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

bool sta_has_ipv4()
{
    auto* sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif == nullptr) return false;

    esp_netif_ip_info_t info{};
    return esp_netif_get_ip_info(sta_netif, &info) == ESP_OK && info.ip.addr != 0U;
}

esp_err_t disable_setup_ap()
{
    // esp_wifi_set_mode() is idempotent for the current STA-only state, so there
    // is no need to query the mode first. Keeping this path minimal also keeps
    // the ESP-IDF host mock aligned with the production call surface.
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
        if (!sta_has_ipv4()) return ESP_ERR_INVALID_STATE;
        last_error = disable_setup_ap();
        if (last_error == ESP_OK) return ESP_OK;
        vTaskDelay(kDisableRetryDelay);
    }
    return last_error;
}

void setup_ap_guard_task(void*)
{
    bool waiting_for_sta_logged = false;

    for (;;) {
        while (access_runtime::bootstrap_allowed()) {
            waiting_for_sta_logged = false;
            vTaskDelay(kPollDelay);
        }

        // The bootstrap HTTP response must have time to leave the socket before
        // any transport change. A failed Admin persist rolls bootstrap back to
        // allowed=true, so always re-check after this grace period.
        vTaskDelay(kPostBootstrapResponseDelay);
        if (access_runtime::bootstrap_allowed()) continue;

        // Never strand the installer. Admin existence alone is not sufficient
        // proof that the station handover succeeded. Keep the setup AP alive
        // until the STA interface actually owns an IPv4 address. This also
        // provides a recovery path after reboot when the configured WLAN is
        // temporarily unavailable.
        if (!sta_has_ipv4()) {
            if (!waiting_for_sta_logged) {
                ESP_LOGW(kTag, "Admin ready but STA has no IPv4; keeping setup AP active until network handover succeeds");
                waiting_for_sta_logged = true;
            }
            vTaskDelay(kStaReadyPollDelay);
            continue;
        }

        ESP_LOGI(kTag, "STA IPv4 ready; closing setup AP");
        const auto error = disable_setup_ap_with_retries();
        if (error == ESP_ERR_INVALID_STATE) {
            waiting_for_sta_logged = false;
            vTaskDelay(kStaReadyPollDelay);
            continue;
        }
        if (error != ESP_OK) {
            ESP_LOGE(kTag, "Setup AP remained enabled after STA became reachable: %s", esp_err_to_name(error));
            vTaskDelay(kDisableRetryDelay);
            continue;
        }
        vTaskDelete(nullptr);
    }
}

}  // namespace

esp_err_t start_setup_ap_guard()
{
    // NetworkHttp starts AP+STA so first-time setup is always reachable. The AP
    // guard now owns the transition to STA-only and performs it only after an
    // Admin exists AND the station interface has a usable IPv4 address.
    if (access_runtime::bootstrap_allowed() && !configure_setup_captive_portal()) {
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
