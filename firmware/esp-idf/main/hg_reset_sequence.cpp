#include "hg_reset_sequence.hpp"

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
constexpr const char* kNvsNamespace = "hg_rstseq";
constexpr const char* kNvsCountKey = "count";
constexpr std::uint8_t kRequiredPresses = 3U;
constexpr TickType_t kSequenceWindowTicks = pdMS_TO_TICKS(4500);
constexpr std::uint32_t kSequenceWindowTaskStackBytes = 4096U;
constexpr int kResetRgbGpio = 48;
constexpr unsigned kResetWhiteMs = 5000U;

esp_err_t load_sequence_count(std::uint8_t& count) {
    count = 0U;
    nvs_handle_t handle{};
    const auto open_error = nvs_open(kNvsNamespace, NVS_READONLY, &handle);
    if (open_error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (open_error != ESP_OK) return open_error;

    auto error = nvs_get_u8(handle, kNvsCountKey, &count);
    nvs_close(handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        count = 0U;
        return ESP_OK;
    }
    return error;
}

esp_err_t store_sequence_count(std::uint8_t count) {
    nvs_handle_t handle{};
    auto error = nvs_open(kNvsNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;

    if (count == 0U) {
        error = nvs_erase_key(handle, kNvsCountKey);
        if (error == ESP_ERR_NVS_NOT_FOUND) error = ESP_OK;
    } else {
        error = nvs_set_u8(handle, kNvsCountKey, count);
    }

    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

void clear_sequence_after_window(void*) {
    vTaskDelay(kSequenceWindowTicks);
    const auto error = store_sequence_count(0U);
    if (error == ESP_OK) {
        ESP_LOGI(kTag, "RST sequence window expired; NVS counter cleared");
    } else {
        ESP_LOGE(kTag, "Cannot clear RST sequence counter: %s", esp_err_to_name(error));
    }
    vTaskDelete(nullptr);
}

bool arm_sequence_expiry_task() {
    // NVS commit + ESP logging needs materially more stack than the original
    // 1536-byte worker. The smaller stack overflowed on the real ESP32-S3
    // before it could clear the 4.5-second reset window.
    if (xTaskCreate(
            &clear_sequence_after_window,
            "hg_rst_window",
            kSequenceWindowTaskStackBytes,
            nullptr,
            3,
            nullptr) == pdPASS) {
        return true;
    }

    const auto clear_error = store_sequence_count(0U);
    ESP_LOGE(kTag,
             "Cannot arm triple-RST expiry window; sequence cancelled%s%s",
             clear_error == ESP_OK ? "" : ": ",
             clear_error == ESP_OK ? "" : esp_err_to_name(clear_error));
    return false;
}

}  // namespace

bool handle_triple_rst_factory_reset() {
    const auto reason = esp_reset_reason();
    const bool physical_reset = hg::physical_reset_candidate(
        reason == ESP_RST_EXT,
        reason == ESP_RST_POWERON);

    std::uint8_t previous_count = 0U;
    const auto load_error = load_sequence_count(previous_count);
    if (load_error != ESP_OK) {
        ESP_LOGE(kTag,
                 "Cannot load RST sequence counter; ignoring reset sequence: %s",
                 esp_err_to_name(load_error));
        return false;
    }

    if (!physical_reset) {
        if (previous_count != 0U) {
            const auto clear_error = store_sequence_count(0U);
            if (clear_error != ESP_OK) {
                ESP_LOGE(kTag,
                         "Cannot clear stale RST sequence after reset reason=%d: %s",
                         static_cast<int>(reason),
                         esp_err_to_name(clear_error));
            }
        }
        ESP_LOGI(kTag,
                 "Reset reason=%d is not a physical RST candidate; sequence cleared",
                 static_cast<int>(reason));
        return false;
    }

    const auto step = hg::advance_reset_sequence(
        previous_count,
        true,
        kRequiredPresses);

    if (!step.trigger_factory_reset) {
        const auto save_error = store_sequence_count(step.count);
        if (save_error != ESP_OK) {
            ESP_LOGE(kTag,
                     "Cannot persist physical RST sequence %u/%u: %s",
                     static_cast<unsigned>(step.count),
                     static_cast<unsigned>(kRequiredPresses),
                     esp_err_to_name(save_error));
            return false;
        }

        ESP_LOGW(kTag,
                 "Physical RST sequence: %u/%u (reason=%d, persisted in NVS)",
                 static_cast<unsigned>(step.count),
                 static_cast<unsigned>(kRequiredPresses),
                 static_cast<int>(reason));
        arm_sequence_expiry_task();
        return false;
    }

    // Consume the sequence before any destructive work so an interruption
    // during LED feedback or flash erasure cannot re-trigger on the next boot.
    const auto clear_error = store_sequence_count(0U);
    if (clear_error != ESP_OK) {
        ESP_LOGE(kTag,
                 "Triple RST detected but sequence counter cannot be cleared; Factory Reset cancelled: %s",
                 esp_err_to_name(clear_error));
        return false;
    }

    ESP_LOGW(kTag, "Triple RST detected: WHITE RGB for 5 seconds before Factory Reset");

    // GPIO48 is the confirmed onboard WS2812 on this HomeGuard-S3 board.
    // LED failure is logged but must not block the requested Factory Reset.
    const auto rgb_error = RgbDiagnostic::test_white(kResetRgbGpio, kResetWhiteMs);
    if (rgb_error != ESP_OK) {
        ESP_LOGE(kTag, "Factory-reset white RGB feedback failed: %s", esp_err_to_name(rgb_error));
    }

    const auto report = FactoryResetManager{}.erase_mutable_state();
    if (!report.ok()) {
        ESP_LOGE(kTag,
                 "Triple-RST Factory Reset failed: access=%d wifi=%d cloud=%d config=%d provisioning=%d commissioning=%d",
                 report.access,
                 report.wifi,
                 report.cloud,
                 report.controller_config,
                 report.provisioning,
                 report.commissioning);
        // The reset operation may already have erased a subset of namespaces.
        // Never return from app_main into an inert partial-boot state: reboot
        // and let the normal fail-closed/bootstrap paths recover consistently.
        ESP_LOGW(kTag, "Factory Reset was partial; rebooting into recovery-safe boot path");
        esp_restart();
        return true;
    }

    ESP_LOGW(kTag, "Triple-RST Factory Reset complete; rebooting clean");
    esp_restart();
    return true;
}

}  // namespace homeguard::idf
