#include "hg_nvs_recovery.hpp"

#include "hg_board_hw678.hpp"
#include "hg_rgb_diagnostic.hpp"
#include "homeguard/reset_sequence.hpp"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include <cstdint>

namespace homeguard::idf {
namespace {

constexpr const char* kTag = "hg_nvs_recovery";
constexpr std::uint8_t kRequiredHolds = 3U;
constexpr TickType_t kHoldTicks = pdMS_TO_TICKS(1500);
constexpr TickType_t kDebounceTicks = pdMS_TO_TICKS(40);
constexpr TickType_t kPollTicks = pdMS_TO_TICKS(20);
constexpr TickType_t kSequenceTimeoutTicks = pdMS_TO_TICKS(5000);
constexpr TickType_t kSuccessRedTicks = pdMS_TO_TICKS(5000);

bool pressed() {
    return gpio_get_level(board::kServiceButton) == 0;
}

[[noreturn]] void restart_recovery() {
    vTaskDelay(pdMS_TO_TICKS(250));
    esp_restart();
    for (;;) vTaskDelay(portMAX_DELAY);
}

}  // namespace

[[noreturn]] void enter_nvs_recovery_mode(esp_err_t init_error) {
    ESP_LOGE(kTag,
             "NVS init failed (%s). FAIL-CLOSED physical recovery only: hold GPIO%d 1.5s until WHITE, release, repeat 3x",
             esp_err_to_name(init_error),
             static_cast<int>(board::kServiceButton));

    gpio_config_t config{};
    config.pin_bit_mask = 1ULL << static_cast<unsigned>(board::kServiceButton);
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;
    if (gpio_config(&config) != ESP_OK) restart_recovery();

    std::uint8_t confirmed_holds = 0U;
    bool raw_pressed = pressed();
    bool stable_pressed = raw_pressed;
    bool hold_confirmed = false;
    TickType_t now = xTaskGetTickCount();
    TickType_t raw_changed_at = now;
    TickType_t press_started_at = now;
    TickType_t last_confirmed_release_at = now;

    for (;;) {
        now = xTaskGetTickCount();
        const bool sampled = pressed();
        if (sampled != raw_pressed) {
            raw_pressed = sampled;
            raw_changed_at = now;
        }

        if (raw_pressed != stable_pressed && (now - raw_changed_at) >= kDebounceTicks) {
            stable_pressed = raw_pressed;
            if (stable_pressed) {
                const bool timed_out = confirmed_holds != 0U &&
                    (now - last_confirmed_release_at) > kSequenceTimeoutTicks;
                confirmed_holds = hg::expire_reset_gesture(confirmed_holds, timed_out);
                press_started_at = now;
                hold_confirmed = false;
            } else {
                if (hold_confirmed) {
                    (void)RgbDiagnostic::off(board::kOnboardRgb);
                    const auto step = hg::advance_confirmed_hold(confirmed_holds, true, kRequiredHolds);
                    confirmed_holds = step.count;
                    last_confirmed_release_at = now;
                    ESP_LOGW(kTag, "Recovery hold confirmed: %u/%u",
                             static_cast<unsigned>(step.trigger_factory_reset ? kRequiredHolds : confirmed_holds),
                             static_cast<unsigned>(kRequiredHolds));
                    if (step.trigger_factory_reset) {
                        ESP_LOGW(kTag, "Physical recovery gesture accepted; erasing full NVS partition");
                        const auto erase_error = nvs_flash_erase();
                        if (erase_error != ESP_OK) {
                            ESP_LOGE(kTag, "NVS recovery erase failed: %s", esp_err_to_name(erase_error));
                            (void)RgbDiagnostic::off(board::kOnboardRgb);
                            restart_recovery();
                        }
                        ESP_LOGW(kTag, "Recovery erase complete; RED confirmation for 5 seconds");
                        if (RgbDiagnostic::set_red(board::kOnboardRgb) == ESP_OK) {
                            vTaskDelay(kSuccessRedTicks);
                            (void)RgbDiagnostic::off(board::kOnboardRgb);
                        }
                        restart_recovery();
                    }
                }
                hold_confirmed = false;
            }
        }

        if (stable_pressed && !hold_confirmed && (now - press_started_at) >= kHoldTicks) {
            if (RgbDiagnostic::set_white(board::kOnboardRgb) == ESP_OK) hold_confirmed = true;
        }

        if (!stable_pressed && confirmed_holds != 0U) {
            const bool timed_out = (now - last_confirmed_release_at) > kSequenceTimeoutTicks;
            confirmed_holds = hg::expire_reset_gesture(confirmed_holds, timed_out);
        }
        vTaskDelay(kPollTicks);
    }
}

}  // namespace homeguard::idf
