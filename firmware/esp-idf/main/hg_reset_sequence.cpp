#include "hg_reset_sequence.hpp"

#include "hg_factory_reset.hpp"

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

bool is_external_rst(esp_reset_reason_t reason) {
    return reason == ESP_RST_EXT;
}

}  // namespace

bool handle_triple_rst_factory_reset() {
    const auto reason = esp_reset_reason();

    if (g_reset_magic != kMagic) {
        reset_sequence_state();
    }

    // Only the physical EN/RST path participates. Power-on, software restart,
    // watchdog, panic and brownout explicitly cancel any partial sequence.
    if (!is_external_rst(reason)) {
        reset_sequence_state();
        return false;
    }

    if (g_external_reset_count < 0xffU) {
        ++g_external_reset_count;
    }

    ESP_LOGW(kTag, "External RST sequence: %u/%u",
             static_cast<unsigned>(g_external_reset_count),
             static_cast<unsigned>(kRequiredPresses));

    if (g_external_reset_count < kRequiredPresses) {
        if (xTaskCreate(
                &clear_sequence_after_window,
                "hg_rst_window",
                1536,
                nullptr,
                3,
                nullptr) != pdPASS) {
            // Fail safe: inability to arm the expiry window must not leave a
            // latent reset count alive indefinitely.
            reset_sequence_state();
            ESP_LOGE(kTag, "Cannot arm triple-RST expiry window; sequence cancelled");
        }
        return false;
    }

    // Consume the sequence before touching flash so a reboot during erase can
    // never recursively trigger another factory reset.
    reset_sequence_state();
    ESP_LOGW(kTag, "Triple RST detected: erasing mutable HomeGuard state");

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
