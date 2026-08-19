#include "hg_reset_sequence.hpp"

#include "hg_board_hw678.hpp"
#include "hg_factory_reset.hpp"
#include "hg_rgb_diagnostic.hpp"
#include "homeguard/reset_sequence.hpp"

#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include <cstdint>

namespace homeguard::idf {
namespace {

constexpr const char* kTag = "hg_rst_sequence";
constexpr const char* kControlNamespace = "hg_rstctl";
constexpr const char* kPendingKey = "pending";
constexpr std::uint8_t kPendingValue = 1U;
constexpr std::uint8_t kRequiredHolds = 3U;
constexpr TickType_t kHoldTicks = pdMS_TO_TICKS(1500);
constexpr TickType_t kDebounceTicks = pdMS_TO_TICKS(40);
constexpr TickType_t kPollTicks = pdMS_TO_TICKS(20);
constexpr TickType_t kSequenceTimeoutTicks = pdMS_TO_TICKS(5000);
constexpr TickType_t kSuccessRedTicks = pdMS_TO_TICKS(5000);
constexpr std::uint32_t kResetTaskStackBytes = 4096U;

bool service_button_pressed() {
    return gpio_get_level(board::kServiceButton) == 0;
}

esp_err_t set_pending_reset(bool pending) {
    nvs_handle_t handle{};
    auto error = nvs_open(kControlNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;

    if (pending) {
        error = nvs_set_u8(handle, kPendingKey, kPendingValue);
    } else {
        error = nvs_erase_key(handle, kPendingKey);
        if (error == ESP_ERR_NVS_NOT_FOUND) error = ESP_OK;
    }
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

esp_err_t read_pending_reset(bool& pending) {
    pending = false;
    nvs_handle_t handle{};
    const auto open_error = nvs_open(kControlNamespace, NVS_READONLY, &handle);
    if (open_error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (open_error != ESP_OK) return open_error;

    std::uint8_t value = 0U;
    auto error = nvs_get_u8(handle, kPendingKey, &value);
    nvs_close(handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (error != ESP_OK) return error;
    pending = value == kPendingValue;
    return ESP_OK;
}

void perform_early_boot_factory_reset() {
    const auto clear_error = set_pending_reset(false);
    if (clear_error != ESP_OK) {
        ESP_LOGE(kTag, "Cannot consume pending Factory Reset request: %s", esp_err_to_name(clear_error));
        esp_restart();
        return;
    }

    ESP_LOGW(kTag, "Pending Factory Reset accepted during early boot; erasing mutable state");
    const auto report = FactoryResetManager{}.erase_mutable_state();
    if (!report.ok()) {
        ESP_LOGE(kTag,
                 "Factory Reset failed: access=%d wifi=%d cloud=%d config=%d provisioning=%d commissioning=%d",
                 report.access,
                 report.wifi,
                 report.cloud,
                 report.controller_config,
                 report.provisioning,
                 report.commissioning);
        (void)RgbDiagnostic::off(board::kOnboardRgb);
        esp_restart();
        return;
    }

    ESP_LOGW(kTag, "Factory Reset complete; RED RGB confirmation for 5 seconds");
    const auto rgb_error = RgbDiagnostic::set_red(board::kOnboardRgb);
    if (rgb_error != ESP_OK) {
        ESP_LOGE(kTag, "Factory-reset RED confirmation failed: %s", esp_err_to_name(rgb_error));
    } else {
        vTaskDelay(kSuccessRedTicks);
        (void)RgbDiagnostic::off(board::kOnboardRgb);
    }
    ESP_LOGW(kTag, "Factory Reset confirmed; rebooting clean");
    esp_restart();
}

void stage_factory_reset_and_reboot() {
    (void)RgbDiagnostic::off(board::kOnboardRgb);
    const auto error = stage_factory_reset_request();
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "Cannot stage Factory Reset request; no destructive reset performed: %s", esp_err_to_name(error));
        return;
    }
    ESP_LOGW(kTag, "Three confirmed holds complete; Factory Reset staged for safe early boot");
    esp_restart();
}

void service_button_reset_task(void*) {
    std::uint8_t confirmed_holds = 0U;
    bool raw_pressed = service_button_pressed();
    bool stable_pressed = raw_pressed;
    bool hold_confirmed = false;
    TickType_t now = xTaskGetTickCount();
    TickType_t raw_changed_at = now;
    TickType_t press_started_at = now;
    TickType_t last_confirmed_release_at = now;

    ESP_LOGI(kTag,
             "Service-button Factory Reset armed on GPIO%d: hold 1.5 s until WHITE, release, repeat 3x",
             static_cast<int>(board::kServiceButton));

    for (;;) {
        now = xTaskGetTickCount();
        const bool sampled_pressed = service_button_pressed();
        if (sampled_pressed != raw_pressed) {
            raw_pressed = sampled_pressed;
            raw_changed_at = now;
        }

        if (raw_pressed != stable_pressed && (now - raw_changed_at) >= kDebounceTicks) {
            stable_pressed = raw_pressed;
            if (stable_pressed) {
                const bool timed_out = confirmed_holds != 0U &&
                    (now - last_confirmed_release_at) > kSequenceTimeoutTicks;
                const auto retained = hg::expire_reset_gesture(confirmed_holds, timed_out);
                if (retained != confirmed_holds) {
                    ESP_LOGI(kTag, "Factory-reset gesture timed out; sequence cleared");
                    confirmed_holds = retained;
                }
                press_started_at = now;
                hold_confirmed = false;
                ESP_LOGI(kTag, "Service button pressed; waiting for hold threshold");
            } else {
                if (hold_confirmed) {
                    (void)RgbDiagnostic::off(board::kOnboardRgb);
                    const auto step = hg::advance_confirmed_hold(confirmed_holds, true, kRequiredHolds);
                    confirmed_holds = step.count;
                    last_confirmed_release_at = now;
                    ESP_LOGW(kTag,
                             "Factory-reset hold confirmed: %u/%u",
                             static_cast<unsigned>(step.trigger_factory_reset ? kRequiredHolds : confirmed_holds),
                             static_cast<unsigned>(kRequiredHolds));
                    if (step.trigger_factory_reset) stage_factory_reset_and_reboot();
                } else {
                    ESP_LOGI(kTag, "Short/unconfirmed service-button press ignored");
                }
                hold_confirmed = false;
            }
        }

        if (stable_pressed && !hold_confirmed && (now - press_started_at) >= kHoldTicks) {
            const auto rgb_error = RgbDiagnostic::set_white(board::kOnboardRgb);
            if (rgb_error != ESP_OK) {
                ESP_LOGE(kTag, "Cannot show WHITE hold confirmation; hold not armed: %s", esp_err_to_name(rgb_error));
            } else {
                hold_confirmed = true;
                ESP_LOGW(kTag, "Hold threshold reached; WHITE means release now");
            }
        }

        if (!stable_pressed && confirmed_holds != 0U) {
            const bool timed_out = (now - last_confirmed_release_at) > kSequenceTimeoutTicks;
            const auto retained = hg::expire_reset_gesture(confirmed_holds, timed_out);
            if (retained != confirmed_holds) {
                ESP_LOGI(kTag, "Factory-reset gesture timed out; sequence cleared");
                confirmed_holds = retained;
            }
        }
        vTaskDelay(kPollTicks);
    }
}

}  // namespace

bool handle_pending_factory_reset() {
    bool pending = false;
    const auto error = read_pending_reset(pending);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "Cannot read pending Factory Reset request: %s", esp_err_to_name(error));
        return false;
    }
    if (!pending) return false;
    perform_early_boot_factory_reset();
    return true;
}

esp_err_t stage_factory_reset_request() {
    return set_pending_reset(true);
}

esp_err_t start_service_button_factory_reset() {
    // app_main invokes this immediately after nvs_flash_init(), before network,
    // HTTP, cloud, or other mutable-state users. A staged reset is consumed here.
    if (handle_pending_factory_reset()) return ESP_OK;

    gpio_config_t config{};
    config.pin_bit_mask = 1ULL << static_cast<unsigned>(board::kServiceButton);
    config.mode = GPIO_MODE_INPUT;
    config.pull_up_en = GPIO_PULLUP_ENABLE;
    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.intr_type = GPIO_INTR_DISABLE;

    auto error = gpio_config(&config);
    if (error != ESP_OK) return error;
    if (xTaskCreate(&service_button_reset_task,
                    "hg_rst_button",
                    kResetTaskStackBytes,
                    nullptr,
                    5,
                    nullptr) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

}  // namespace homeguard::idf
