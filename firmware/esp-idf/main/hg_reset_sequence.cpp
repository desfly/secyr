#include "hg_reset_sequence.hpp"

#include "hg_factory_reset.hpp"
#include "hg_rgb_diagnostic.hpp"
#include "homeguard/reset_sequence.hpp"

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdint>

namespace homeguard::idf {
namespace {

constexpr const char* kTag = "hg_rst_sequence";
constexpr std::uint32_t kMagic = 0x48524733U;  // "HRG3"
constexpr std::uint8_t kRequiredPresses = 3U;
constexpr TickType_t kSequenceWindowTicks = pdMS_TO_TICKS(4500);
constexpr int kResetRgbGpio = 48;
constexpr unsigned kResetWhiteMs = 5000U;

RTC_NOINIT_ATTR std::uint32_t g_reset_magic;
RTC_NOINIT_ATTR std::uint8_t g_external_reset_count;

void clear_sequence_after_window(void*) {
    vTaskDelay(kSequenceWindowTicks);
    g_reset_magic = kMagic;
    g_external_reset_count = 0U;
    vTaskDelete(nullptr);
}

void reset_sequence_state() {
    g_reset_magic = kMagic;
    g_external_reset_count = 0U;
}

}  // namespace

bool handle_triple_rst_factory_reset() {
    const bool rtc_state_was_valid = g_reset_magic == kMagic;
    if (!rtc_state_was_valid) reset_sequence_state();

    const auto reason = esp_reset_reason();
    const bool reset_press = hg::reset_press_detected(
        rtc_state_was_valid,
        reason == ESP_RST_EXT,
        reason == ESP_RST_POWERON);

    const auto step = hg::advance_reset_sequence(
        g_external_reset_count,
        reset_press,
        kRequiredPresses);
    g_external_reset_count = step.count;

    if (!step.trigger_factory_reset) {
        if (g_external_reset_count == 0U) {
            ESP_LOGI(kTag,
                     "Reset reason=%d ignored; RST sequence cleared",
                     static_cast<int>(reason));
            return false;
        }

        ESP_LOGW(kTag, "Physical RST sequence: %u/%u (reason=%d)",
                 static_cast<unsigned>(g_external_reset_count),
                 static_cast<unsigned>(kRequiredPresses),
                 static_cast<int>(reason));
        if (xTaskCreate(
                &clear_sequence_after_window,
                "hg_rst_window",
                1536,
                nullptr,
                3,
                nullptr) != pdPASS) {
            reset_sequence_state();
            ESP_LOGE(kTag, "Cannot arm triple-RST expiry window; sequence cancelled");
        }
        return false;
    }

    // Consume before feedback/erasing so a reset during flash operations cannot loop.
    reset_sequence_state();
    ESP_LOGW(kTag, "Triple RST detected: erasing mutable HomeGuard state");

    // Confirm the destructive reset locally. GPIO48 is the confirmed onboard
    // WS2812 on this HomeGuard-S3 board. Failure to light must never block the
    // actual factory reset.
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
        return true;
    }

    ESP_LOGW(kTag, "Triple-RST Factory Reset complete; rebooting clean");
    esp_restart();
    return true;
}

}  // namespace homeguard::idf
