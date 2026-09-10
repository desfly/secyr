#include "hg_relay_runtime.hpp"

#include "hg_board_hw678.hpp"
#include "hg_zone_monitor.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdint>

namespace homeguard::idf {
namespace {
constexpr const char* kTag = "relay_runtime";
constexpr TickType_t kPollTicks = pdMS_TO_TICKS(100);
RelayRuntime* g_active_runtime = nullptr;

bool configure_safe_output(gpio_num_t pin)
{
    if (gpio_set_level(pin, 0) != ESP_OK) return false;
    if (gpio_set_direction(pin, GPIO_MODE_OUTPUT) != ESP_OK) return false;
    return gpio_set_level(pin, 0) == ESP_OK;
}

bool write_pin(gpio_num_t pin, bool active)
{
    return gpio_set_level(pin, active ? 1 : 0) == ESP_OK;
}
}

RelayRuntime* RelayRuntime::active_runtime() noexcept
{
    return g_active_runtime;
}

std::uint64_t RelayRuntime::now_ms()
{
    return static_cast<std::uint64_t>(esp_timer_get_time() / 1000ULL);
}

esp_err_t RelayRuntime::start(Mcp23017* unused_expander, ZoneMonitor* zones)
{
    (void)unused_expander;
    return start(zones);
}

esp_err_t RelayRuntime::start(ZoneMonitor* zones)
{
    if (zones == nullptr) return ESP_ERR_INVALID_ARG;
    if (mutex_ != nullptr) return ESP_ERR_INVALID_STATE;

    if (!configure_safe_output(board::kRelayLight) ||
        !configure_safe_output(board::kRelayLock) ||
        !configure_safe_output(board::kRelayValve1) ||
        !configure_safe_output(board::kRelayValve2)) {
        gpio_set_level(board::kRelayLight, 0);
        gpio_set_level(board::kRelayLock, 0);
        gpio_set_level(board::kRelayValve1, 0);
        gpio_set_level(board::kRelayValve2, 0);
        return ESP_FAIL;
    }

    mutex_ = xSemaphoreCreateMutex();
    if (mutex_ == nullptr) return ESP_ERR_NO_MEM;
    zones_ = zones;

    if (xTaskCreate(&RelayRuntime::task_entry, "hg_relays", 4096, this, 5, nullptr) != pdPASS) {
        // Outputs have already been forced safe. Keep the allocated mutex here:
        // this is a terminal startup failure and avoids depending on delete
        // primitives that are intentionally absent from the host mock layer.
        force_safe_locked();
        zones_ = nullptr;
        return ESP_ERR_NO_MEM;
    }

    g_active_runtime = this;
    ESP_LOGI(kTag,
        "Direct relay GPIO ready: LIGHT=%d LOCK=%d VALVE1=%d VALVE2=%d",
        static_cast<int>(board::kRelayLight),
        static_cast<int>(board::kRelayLock),
        static_cast<int>(board::kRelayValve1),
        static_cast<int>(board::kRelayValve2));
    return ESP_OK;
}

bool RelayRuntime::force_safe_locked()
{
    const bool ok = write_pin(board::kRelayLight, false) &
                    write_pin(board::kRelayLock, false) &
                    write_pin(board::kRelayValve1, false) &
                    write_pin(board::kRelayValve2, false);
    light_active_ = false;
    manual_light_ = false;
    automatic_light_ = false;
    lock_active_ = false;
    valve1_active_ = false;
    valve2_active_ = false;
    light_deadline_ms_ = 0;
    lock_deadline_ms_ = 0;
    return ok;
}

bool RelayRuntime::write_state_locked(bool light, bool lock, bool valve1, bool valve2)
{
    if (!write_pin(board::kRelayLight, light) ||
        !write_pin(board::kRelayLock, lock) ||
        !write_pin(board::kRelayValve1, valve1) ||
        !write_pin(board::kRelayValve2, valve2)) {
        force_safe_locked();
        return false;
    }
    light_active_ = light;
    lock_active_ = lock;
    valve1_active_ = valve1;
    valve2_active_ = valve2;
    return true;
}

bool RelayRuntime::set_manual_light(bool active)
{
    if (mutex_ == nullptr) return false;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(250)) != pdTRUE) return false;
    manual_light_ = active;
    const bool requested = manual_light_ || automatic_light_;
    const bool ok = requested == light_active_ ||
        write_state_locked(requested, lock_active_, valve1_active_, valve2_active_);
    xSemaphoreGive(mutex_);
    return ok;
}

bool RelayRuntime::request_lock_pulse()
{
    if (mutex_ == nullptr) return false;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(250)) != pdTRUE) return false;

    bool accepted = true;
    if (!lock_active_) {
        const auto now = now_ms();
        accepted = write_state_locked(light_active_, true, valve1_active_, valve2_active_);
        if (accepted) {
            lock_deadline_ms_ = now + kLockPulseMs;
            ESP_LOGI(kTag, "Lock relay GPIO%d ON for %u ms",
                static_cast<int>(board::kRelayLock), static_cast<unsigned>(kLockPulseMs));
        }
    } else {
        ESP_LOGI(kTag, "Lock pulse already active; repeat ignored without extending deadline");
    }

    xSemaphoreGive(mutex_);
    return accepted;
}

bool RelayRuntime::set_valve(std::uint8_t index, bool active)
{
    if (index > 1U || mutex_ == nullptr) return false;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(250)) != pdTRUE) return false;

    const bool valve1 = index == 0U ? active : valve1_active_;
    const bool valve2 = index == 1U ? active : valve2_active_;
    const bool ok = write_state_locked(light_active_, lock_active_, valve1, valve2);
    xSemaphoreGive(mutex_);
    return ok;
}

RelayRuntimeState RelayRuntime::state()
{
    RelayRuntimeState out{};
    if (mutex_ == nullptr) return out;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(250)) != pdTRUE) return out;
    const auto now = now_ms();
    out.light_active = light_active_;
    out.light_manual = manual_light_;
    out.light_automatic = automatic_light_;
    out.lock_active = lock_active_;
    out.valve1_active = valve1_active_;
    out.valve2_active = valve2_active_;
    if (lock_active_ && lock_deadline_ms_ > now) {
        const auto remaining = lock_deadline_ms_ - now;
        out.lock_remaining_ms = remaining > UINT32_MAX ? UINT32_MAX : static_cast<std::uint32_t>(remaining);
    }
    xSemaphoreGive(mutex_);
    return out;
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
            if (!automatic_light_ && alarm) {
                automatic_light_ = true;
                light_deadline_ms_ = now + kLightCycleMs;
                ESP_LOGI(kTag, "Z1/Z2 alarm: automatic light cycle ON for 60 seconds");
            } else if (automatic_light_ && now >= light_deadline_ms_) {
                if (alarm) {
                    light_deadline_ms_ = now + kLightCycleMs;
                    ESP_LOGI(kTag, "Z1/Z2 still in alarm: next 60 second light cycle");
                } else {
                    automatic_light_ = false;
                    light_deadline_ms_ = 0;
                    ESP_LOGI(kTag, "Automatic light cycle complete and Z1/Z2 normal");
                }
            }

            const bool next_light = manual_light_ || automatic_light_;
            bool next_lock = lock_active_;
            if (lock_active_ && now >= lock_deadline_ms_) {
                next_lock = false;
                lock_deadline_ms_ = 0;
                ESP_LOGI(kTag, "Lock 5 second pulse complete: lock relay OFF");
            }

            if (next_light != light_active_ || next_lock != lock_active_) {
                if (!write_state_locked(next_light, next_lock, valve1_active_, valve2_active_)) {
                    ESP_LOGE(kTag, "Direct relay GPIO write failed; all four relay outputs forced OFF");
                }
            }
            xSemaphoreGive(mutex_);
        }

        vTaskDelay(kPollTicks);
    }
}

}  // namespace homeguard::idf
