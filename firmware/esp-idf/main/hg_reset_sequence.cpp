#include "hg_reset_sequence.hpp"

#include "hg_board_hw678.hpp"
#include "hg_factory_reset.hpp"
#include "hg_rgb_diagnostic.hpp"
#include "homeguard/reset_sequence.hpp"

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
constexpr const char* kSequenceNamespace = "hg_rstseq";
constexpr const char* kSequenceCountKey = "count";
constexpr const char* kBootMarkerKey = "boot_seen";
constexpr std::uint8_t kBootMarkerValue = 0xA5U;
constexpr TickType_t kStepWhiteTicks = pdMS_TO_TICKS(hg::kFactoryResetStepWhiteMs);
constexpr TickType_t kSequenceWindowTicks = pdMS_TO_TICKS(hg::kFactoryResetSequenceWindowMs);
constexpr TickType_t kSuccessRedTicks = pdMS_TO_TICKS(hg::kFactoryResetSuccessRedMs);
constexpr std::uint32_t kResetWorkerStackBytes = 4096U;

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

esp_err_t load_sequence_count(std::uint8_t& count) {
    count = 0U;
    nvs_handle_t handle{};
    const auto open_error = nvs_open(kSequenceNamespace, NVS_READONLY, &handle);
    if (open_error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (open_error != ESP_OK) return open_error;

    auto error = nvs_get_u8(handle, kSequenceCountKey, &count);
    nvs_close(handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        count = 0U;
        return ESP_OK;
    }
    return error;
}

esp_err_t store_sequence_count(std::uint8_t count) {
    nvs_handle_t handle{};
    auto error = nvs_open(kSequenceNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;

    if (count == 0U) {
        error = nvs_erase_key(handle, kSequenceCountKey);
        if (error == ESP_ERR_NVS_NOT_FOUND) error = ESP_OK;
    } else {
        error = nvs_set_u8(handle, kSequenceCountKey, count);
    }

    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

esp_err_t load_boot_marker(bool& valid) {
    valid = false;
    nvs_handle_t handle{};
    const auto open_error = nvs_open(kSequenceNamespace, NVS_READONLY, &handle);
    if (open_error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (open_error != ESP_OK) return open_error;

    std::uint8_t value = 0U;
    auto error = nvs_get_u8(handle, kBootMarkerKey, &value);
    nvs_close(handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (error != ESP_OK) return error;
    valid = value == kBootMarkerValue;
    return ESP_OK;
}

esp_err_t store_boot_marker() {
    nvs_handle_t handle{};
    auto error = nvs_open(kSequenceNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    error = nvs_set_u8(handle, kBootMarkerKey, kBootMarkerValue);
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

void step_feedback_and_timeout_task(void*) {
    vTaskDelay(kStepWhiteTicks);
    (void)RgbDiagnostic::off(board::kOnboardRgb);

    if (kSequenceWindowTicks > kStepWhiteTicks) {
        vTaskDelay(kSequenceWindowTicks - kStepWhiteTicks);
    }

    const auto error = store_sequence_count(0U);
    if (error == ESP_OK) {
        ESP_LOGI(kTag, "Physical RST sequence timed out; counter cleared");
    } else {
        ESP_LOGE(kTag, "Cannot clear physical RST sequence counter: %s", esp_err_to_name(error));
    }
    vTaskDelete(nullptr);
}

bool arm_step_feedback_and_timeout() {
    if (xTaskCreate(
            &step_feedback_and_timeout_task,
            "hg_rst_window",
            kResetWorkerStackBytes,
            nullptr,
            3,
            nullptr) == pdPASS) {
        return true;
    }

    (void)RgbDiagnostic::off(board::kOnboardRgb);
    (void)store_sequence_count(0U);
    ESP_LOGE(kTag, "Cannot arm physical RST timeout worker; sequence cancelled");
    return false;
}

void perform_early_boot_factory_reset() {
    ESP_LOGW(kTag, "Pending Factory Reset accepted during early boot; erasing mutable state");
    const auto report = FactoryResetManager{}.erase_mutable_state();
    if (!report.ok()) {
        ESP_LOGE(kTag,
                 "Factory Reset failed; request remains pending for retry: access=%d wifi=%d cloud=%d config=%d provisioning=%d commissioning=%d",
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

    const auto clear_error = set_pending_reset(false);
    if (clear_error != ESP_OK) {
        ESP_LOGE(kTag,
                 "Factory Reset erased state but cannot consume pending marker: %s; retrying next boot",
                 esp_err_to_name(clear_error));
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

bool handle_physical_rst_factory_reset() {
    if (handle_pending_factory_reset()) return true;

    const auto reason = esp_reset_reason();

    std::uint8_t previous_count = 0U;
    const auto load_count_error = load_sequence_count(previous_count);
    if (load_count_error != ESP_OK) {
        ESP_LOGE(kTag, "Cannot load physical RST sequence: %s", esp_err_to_name(load_count_error));
        return false;
    }

    bool boot_marker_was_valid = false;
    const auto load_marker_error = load_boot_marker(boot_marker_was_valid);
    if (load_marker_error != ESP_OK) {
        ESP_LOGE(kTag, "Cannot load physical RST boot marker: %s", esp_err_to_name(load_marker_error));
        return false;
    }

    // HW-678 evidence: both a true cold start and its physical RST/EN button
    // are reported as POWERON, while RTC_NOINIT does not survive EN reset.
    // Establish one persistent baseline boot after a fresh NVS. Only later
    // POWERON boots can advance the gesture. This makes the first boot after
    // erase/Factory Reset safe and lets the real RST button work thereafter.
    if (!boot_marker_was_valid) {
        const auto marker_error = store_boot_marker();
        if (marker_error != ESP_OK) {
            ESP_LOGE(kTag, "Cannot establish physical RST boot marker: %s", esp_err_to_name(marker_error));
            return false;
        }
        if (previous_count != 0U) {
            (void)store_sequence_count(0U);
        }
        ESP_LOGI(kTag,
                 "Persistent RST boot baseline established; reset reason=%d is not counted",
                 static_cast<int>(reason));
        return false;
    }

    const bool reset_press = hg::reset_press_detected(
        boot_marker_was_valid,
        reason == ESP_RST_EXT,
        reason == ESP_RST_POWERON);

    if (!reset_press) {
        if (previous_count != 0U) {
            const auto clear_error = store_sequence_count(0U);
            if (clear_error != ESP_OK) {
                ESP_LOGE(kTag, "Cannot clear stale physical RST sequence: %s", esp_err_to_name(clear_error));
            }
        }
        ESP_LOGI(kTag, "Reset reason=%d is not a physical RST gesture step", static_cast<int>(reason));
        return false;
    }

    const auto step = hg::advance_reset_sequence(
        previous_count,
        true,
        hg::kFactoryResetRequiredRstPresses);

    // Every accepted RST/EN step gets WHITE acknowledgement. If the RGB cannot
    // show WHITE, cancel the sequence so an invisible step can never count.
    const auto white_error = RgbDiagnostic::set_white(board::kOnboardRgb);
    if (white_error != ESP_OK) {
        ESP_LOGE(kTag,
                 "Cannot show WHITE physical-RST acknowledgement; sequence cancelled: %s",
                 esp_err_to_name(white_error));
        (void)store_sequence_count(0U);
        return false;
    }

    if (!step.trigger_factory_reset) {
        const auto save_error = store_sequence_count(step.count);
        if (save_error != ESP_OK) {
            ESP_LOGE(kTag,
                     "Cannot persist physical RST step %u/%u: %s",
                     static_cast<unsigned>(step.count),
                     static_cast<unsigned>(hg::kFactoryResetRequiredRstPresses),
                     esp_err_to_name(save_error));
            (void)RgbDiagnostic::off(board::kOnboardRgb);
            (void)store_sequence_count(0U);
            return false;
        }

        ESP_LOGW(kTag,
                 "Physical RST accepted: WHITE acknowledgement, step %u/%u",
                 static_cast<unsigned>(step.count),
                 static_cast<unsigned>(hg::kFactoryResetRequiredRstPresses));
        (void)arm_step_feedback_and_timeout();
        return false;
    }

    ESP_LOGW(kTag, "Physical RST accepted: WHITE acknowledgement, step 3/3");
    vTaskDelay(kStepWhiteTicks);
    (void)RgbDiagnostic::off(board::kOnboardRgb);

    const auto clear_error = store_sequence_count(0U);
    if (clear_error != ESP_OK) {
        ESP_LOGE(kTag,
                 "Triple physical RST detected but counter cannot be consumed: %s",
                 esp_err_to_name(clear_error));
        return false;
    }

    const auto stage_error = stage_factory_reset_request();
    if (stage_error != ESP_OK) {
        ESP_LOGE(kTag,
                 "Triple physical RST detected but Factory Reset cannot be staged: %s",
                 esp_err_to_name(stage_error));
        return false;
    }

    ESP_LOGW(kTag, "Three physical RST steps confirmed; Factory Reset staged for safe early boot");
    esp_restart();
    return true;
}

}  // namespace homeguard::idf
