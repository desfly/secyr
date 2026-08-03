#include "watchdog_service.hpp"
#include "esp_log.h"
#include "esp_task_wdt.h"

namespace { constexpr char tag[] = "hg_watchdog"; }

bool WatchdogService::begin(const uint32_t timeout_seconds, const bool panic_on_timeout) {
    if (active_ || timeout_seconds == 0U || timeout_seconds > 300U) return false;
    const esp_task_wdt_config_t config{
        .timeout_ms = timeout_seconds * 1000U,
        .idle_core_mask = 0U,
        .trigger_panic = panic_on_timeout,
    };
    const esp_err_t initialized = esp_task_wdt_init(&config);
    if (initialized != ESP_OK && initialized != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(tag, "task watchdog initialization failed: %d", static_cast<int>(initialized));
        return false;
    }
    const esp_err_t added = esp_task_wdt_add(nullptr);
    if (added != ESP_OK && added != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(tag, "current task could not be registered with watchdog: %d", static_cast<int>(added));
        return false;
    }
    active_ = true;
    ESP_LOGI(tag, "current task registered with %u second watchdog", static_cast<unsigned>(timeout_seconds));
    return true;
}

bool WatchdogService::feed() {
    if (!active_) return false;
    return esp_task_wdt_reset() == ESP_OK;
}

void WatchdogService::stop() {
    if (!active_) return;
    const esp_err_t removed = esp_task_wdt_delete(nullptr);
    if (removed != ESP_OK && removed != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(tag, "watchdog task removal returned %d", static_cast<int>(removed));
    }
    active_ = false;
}
