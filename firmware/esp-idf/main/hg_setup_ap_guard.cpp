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
    bool response_delay_applied = false;

    for (;;) {
        if (access_runtime::bootstrap_allowed()) {
            bootstrap_seen = true;
            response_delay_applied = false;
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
