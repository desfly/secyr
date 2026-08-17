#include "hg_reset_sequence.hpp"

#include "hg_board_hw678.hpp"
#include "hg_factory_reset.hpp"
#include "hg_rgb_diagnostic.hpp"
#include "homeguard/reset_sequence.hpp"

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace homeguard::idf {
namespace {

constexpr const char* kTag = "hg_rst_sequence";
constexpr std::uint32_t kMagic = 0x48524733U;  // "HRG3"
constexpr std::uint8_t kRequiredPresses = 3U;
constexpr TickType_t kSequenceWindowTicks = pdMS_TO_TICKS(4500);
constexpr unsigned kFactoryResetWhiteMs = 5000U;
constexpr TickType_t kFactoryResetColorStepTicks = pdMS_TO_TICKS(200);
constexpr std::size_t kFactoryResetMinColorSteps = 25U;  // 5 seconds.

struct ResetColor {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
};

constexpr std::array<ResetColor, 6> kFactoryResetColors{{
    {0xffU, 0x00U, 0x00U},
    {0xffU, 0x80U, 0x00U},
    {0x00U, 0xffU, 0x00U},
    {0x00U, 0xffU, 0xffU},
    {0x00U, 0x00U, 0xffU},
    {0xffU, 0x00U, 0xffU},
}};

RTC_NOINIT_ATTR std::uint32_t g_reset_magic;
RTC_NOINIT_ATTR std::uint8_t g_external_reset_count;

struct FactoryResetTaskContext {
    TaskHandle_t waiter{nullptr};
    FactoryResetReport report{};
};

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

bool is_physical_rst_boot(esp_reset_reason_t reason) {
    // On ESP32-S3, toggling CHIP_PU/EN (the board RST button) is reported by
    // esp_reset_reason() as ESP_RST_POWERON. Keep ESP_RST_EXT accepted as well
    // for compatibility with targets/boards that expose a distinct external
    // reset reason. RTC_NOINIT keeps the short sequence state across RST/EN,
    // while a real loss of power invalidates the magic and starts from zero.
    return reason == ESP_RST_POWERON || reason == ESP_RST_EXT;
}

void force_reset_rgb_off() {
    const auto error = RgbDiagnostic::off(static_cast<int>(homeguard::board::kRgbLed));
    if (error != ESP_OK) {
        ESP_LOGW(kTag, "Cannot force reset RGB off: %s", esp_err_to_name(error));
    }
}

void factory_reset_worker(void* argument) {
    auto* context = static_cast<FactoryResetTaskContext*>(argument);
    context->report = FactoryResetManager{}.erase_mutable_state();
    xTaskNotifyGive(context->waiter);
    vTaskDelete(nullptr);
}

FactoryResetReport erase_with_color_animation() {
    FactoryResetTaskContext context{};
    context.waiter = xTaskGetCurrentTaskHandle();

    const auto worker_started = xTaskCreate(
        &factory_reset_worker,
        "hg_factory_erase",
        4096,
        &context,
        4,
        nullptr) == pdPASS;

    if (!worker_started) {
        ESP_LOGE(kTag, "Cannot start factory-reset erase worker; falling back to synchronous erase");
        context.report = FactoryResetManager{}.erase_mutable_state();
    }

    bool erase_done = !worker_started;
    std::size_t step = 0U;
    while (step < kFactoryResetMinColorSteps || !erase_done) {
        const auto& color = kFactoryResetColors[step % kFactoryResetColors.size()];
        const auto rgb_error = RgbDiagnostic::set_color(
            static_cast<int>(homeguard::board::kRgbLed),
            color.red,
            color.green,
            color.blue);
        if (rgb_error != ESP_OK) {
            ESP_LOGW(kTag, "Factory-reset RGB color step failed: %s", esp_err_to_name(rgb_error));
        }

        if (worker_started && !erase_done) {
            erase_done = ulTaskNotifyTake(pdTRUE, kFactoryResetColorStepTicks) > 0U;
        } else {
            vTaskDelay(kFactoryResetColorStepTicks);
        }
        ++step;
    }

    force_reset_rgb_off();
    return context.report;
}

void factory_reset_error_blink(void*) {
    const auto gpio = static_cast<int>(homeguard::board::kRgbLed);
    while (true) {
        (void)RgbDiagnostic::set_color(gpio, 0xffU, 0x00U, 0x00U);
        vTaskDelay(pdMS_TO_TICKS(300));
        (void)RgbDiagnostic::off(gpio);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}

void start_factory_reset_error_indicator() {
    if (xTaskCreate(
            &factory_reset_error_blink,
            "hg_rst_error_rgb",
            2048,
            nullptr,
            2,
            nullptr) != pdPASS) {
        ESP_LOGE(kTag, "Cannot start factory-reset error RGB task; latching red instead");
        (void)RgbDiagnostic::set_color(
            static_cast<int>(homeguard::board::kRgbLed),
            0xffU,
            0x00U,
            0x00U);
    }
}

}  // namespace

bool handle_triple_rst_factory_reset() {
    // WS2812 keeps its last latched color across an MCU-only reset. Always clear
    // it first so an ordinary reboot never reproduces the old 5-second white state.
    force_reset_rgb_off();

    if (g_reset_magic != kMagic) reset_sequence_state();

    const auto reset_reason = esp_reset_reason();
    const auto step = hg::advance_reset_sequence(
        g_external_reset_count,
        is_physical_rst_boot(reset_reason),
        kRequiredPresses);
    g_external_reset_count = step.count;

    ESP_LOGI(kTag,
             "Boot reset reason=%d, RST sequence=%u/%u",
             static_cast<int>(reset_reason),
             static_cast<unsigned>(g_external_reset_count),
             static_cast<unsigned>(kRequiredPresses));

    if (!step.trigger_factory_reset) {
        if (g_external_reset_count == 0U) return false;

        ESP_LOGW(kTag, "Physical RST sequence: %u/%u",
                 static_cast<unsigned>(g_external_reset_count),
                 static_cast<unsigned>(kRequiredPresses));
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

    // Consume before erasing so a reset during flash operations cannot loop.
    reset_sequence_state();
    ESP_LOGW(kTag, "Triple RST detected: Factory Reset accepted");

    const auto white_error = RgbDiagnostic::test_white(
        static_cast<int>(homeguard::board::kRgbLed),
        kFactoryResetWhiteMs);
    if (white_error != ESP_OK) {
        ESP_LOGW(kTag, "Factory-reset white confirmation failed: %s", esp_err_to_name(white_error));
    }

    ESP_LOGW(kTag, "Factory Reset: erasing mutable HomeGuard state with RGB indication");
    const auto report = erase_with_color_animation();
    if (!report.ok()) {
        ESP_LOGE(kTag,
                 "Triple-RST Factory Reset failed: access=%d wifi=%d cloud=%d config=%d provisioning=%d commissioning=%d",
                 report.access,
                 report.wifi,
                 report.cloud,
                 report.controller_config,
                 report.provisioning,
                 report.commissioning);
        start_factory_reset_error_indicator();
        return true;
    }

    force_reset_rgb_off();
    ESP_LOGW(kTag, "Triple-RST Factory Reset complete; rebooting clean");
    esp_restart();
    return true;
}

}  // namespace homeguard::idf
