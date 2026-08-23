#include "hg_access_runtime.hpp"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace homeguard::idf {
namespace {

constexpr const char* kTag = "hg_setup_ap";
constexpr TickType_t kPollDelay = pdMS_TO_TICKS(100);
constexpr TickType_t kPostBootstrapResponseDelay = pdMS_TO_TICKS(750);
constexpr unsigned kTaskStackBytes = 2048;
constexpr unsigned kTaskPriority = 3;

void setup_ap_guard_task(void*)
{
    bool bootstrap_seen = false;

    for (;;) {
        if (access_runtime::bootstrap_allowed()) {
            bootstrap_seen = true;
            vTaskDelay(kPollDelay);
            continue;
        }

        wifi_mode_t mode = WIFI_MODE_NULL;
        const auto get_mode_error = esp_wifi_get_mode(&mode);
        if (get_mode_error == ESP_ERR_WIFI_NOT_INIT || mode == WIFI_MODE_NULL) {
            vTaskDelay(kPollDelay);
            continue;
        }
        if (get_mode_error != ESP_OK) {
            ESP_LOGW(kTag, "Cannot inspect Wi-Fi mode while closing setup AP: %s", esp_err_to_name(get_mode_error));
            vTaskDelay(kPollDelay);
            continue;
        }

        if (mode == WIFI_MODE_STA) {
            ESP_LOGI(kTag, "Setup AP already disabled");
            break;
        }

        if (mode != WIFI_MODE_APSTA && mode != WIFI_MODE_AP) {
            vTaskDelay(kPollDelay);
            continue;
        }

        // When the first Admin has just been persisted, leave enough time for
        // the bootstrap HTTP response to reach the browser before the AP drops.
        // On later boots with an existing Admin, close the AP immediately.
        if (bootstrap_seen) vTaskDelay(kPostBootstrapResponseDelay);

        const auto set_mode_error = esp_wifi_set_mode(WIFI_MODE_STA);
        if (set_mode_error == ESP_OK) {
            ESP_LOGI(kTag, "Open setup AP disabled; Wi-Fi is now STA-only");
            break;
        }

        ESP_LOGE(kTag, "Failed to disable setup AP: %s", esp_err_to_name(set_mode_error));
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
