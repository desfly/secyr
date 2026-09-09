#include "hg_relay_runtime.hpp"

#include "hg_mcp23017.hpp"
#include "hg_zone_monitor.hpp"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/task.h"

namespace homeguard::idf {
namespace {
constexpr const char* kTag = "relay_runtime";
constexpr TickType_t kPollTicks = pdMS_TO_TICKS(100);
}

std::uint64_t RelayRuntime::now_ms()
{
    return static_cast<std::uint64_t>(esp_timer_get_time() / 1000ULL);
}

esp_err_t RelayRuntime::start(Mcp23017* expander, ZoneMonitor* zones)
{
    if (expander == nullptr || zones == nullptr || !expander->ready()) return ESP_ERR_INVALID_STATE;
    if (mutex_ != nullptr) return ESP_ERR_INVALID_STATE;

    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) return ESP_ERR_NO_MEM;
    expander_ = expander;
    zones_ = zones;

    if (!write_state_locked(false, false)) {
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
        expander_ = nullptr;
        zones_ = nullptr;
        return ESP_FAIL;
    }

    if (xTaskCreate(&RelayRuntime::task_entry, "hg_relays", 4096, this, 5, nullptr) != pdPASS) {
        expander_->force_safe_outputs();
        vSemaphoreDelete(mutex_);
        mutex_ = nullptr;
        expander_ = nullptr;
        zones_ = nullptr;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

bool RelayRuntime::write_state_locked(bool light, bool lock)
{
    if (expander_ == nullptr) return false;
    std::uint8_t outputs = 0U;
    if (light) outputs |= kLightMask;
    if (lock) outputs |= kLockMask;
    if (expander_->write_outputs(outputs) != ESP_OK) {
        expander_->force_safe_outputs();
        light_active_ = false;
        lock_active_ = false;
        light_deadline_ms_ = 0;
        lock_deadline_ms_ = 0;
        return false;
    }
    light_active_ = light;
    lock_active_ = lock;
    return true;
}

bool RelayRuntime::request_lock_pulse()
{
    if (mutex_ == nullptr || expander_ == nullptr) return false;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(250)) != pdTRUE) return false;

    bool accepted = true;
    if (!lock_active_) {
        const auto now = now_ms();
        accepted = write_state_locked(light_active_, true);
        if (accepted) {
            lock_deadline_ms_ = now + kLockPulseMs;
            ESP_LOGI(kTag, "Lock relay ON for %u ms", static_cast<unsigned>(kLockPulseMs));
        }
    } else {
        ESP_LOGI(kTag, "Lock pulse already active; repeat ignored without extending deadline");
    }

    xSemaphoreGive(mutex_);
    return accepted;
}

void RelayRuntime::task_entry(void* context)
{
    static_cast<RelayRuntime*>(context)->run();
}

void RelayRuntime::run()
{
    while (true) {
        const auto now = now_ms();
        const bool alarm = zones_->alarm_active(0) || zones_->alarm_active(1);

        if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
            bool next_light = light_active_;
            bool next_lock = lock_active_;

            if (!light_active_ && alarm) {
                next_light = true;
                light_deadline_ms_ = now + kLightCycleMs;
                ESP_LOGI(kTag, "Z1/Z2 alarm: light relay ON for a 60 second cycle");
            } else if (light_active_ && now >= light_deadline_ms_) {
                if (alarm) {
                    light_deadline_ms_ = now + kLightCycleMs;
                    ESP_LOGI(kTag, "Z1/Z2 still in alarm: next 60 second light cycle");
                } else {
                    next_light = false;
                    light_deadline_ms_ = 0;
                    ESP_LOGI(kTag, "Light cycle complete and Z1/Z2 normal: light relay OFF");
                }
            }

            if (lock_active_ && now >= lock_deadline_ms_) {
                next_lock = false;
                lock_deadline_ms_ = 0;
                ESP_LOGI(kTag, "Lock 5 second pulse complete: lock relay OFF");
            }

            if (next_light != light_active_ || next_lock != lock_active_) {
                if (!write_state_locked(next_light, next_lock)) {
                    ESP_LOGE(kTag, "MCP23017 relay write failed; all relay outputs forced OFF");
                }
            }
            xSemaphoreGive(mutex_);
        }

        vTaskDelay(kPollTicks);
    }
}

}  // namespace homeguard::idf
