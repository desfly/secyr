#include "hg_reset_sequence.hpp"

#include "hg_board_hw678.hpp"
#include "hg_factory_reset.hpp"
#include "hg_rgb_diagnostic.hpp"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace homeguard::idf {
namespace {

constexpr const char* kTag = "hg_rst_sequence";
constexpr const char* kResetNamespace = "hg_rst";
constexpr const char* kResetCountKey = "count";
constexpr std::uint8_t kRequiredPresses = 3U;
constexpr TickType_t kSequenceWindowTicks = pdMS_TO_TICKS(4500);
constexpr unsigned kFactoryResetWhiteMs = 5000U;
constexpr TickType_t kFactoryResetColorStepTicks = pdMS_TO_TICKS(200);
constexpr std::size_t kFactoryResetMinColorSteps = 25U;

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

struct FactoryResetTaskContext {
    TaskHandle_t waiter{nullptr};
    FactoryResetReport report{};
};

esp_err_t load_reset_count(std::uint8_t& count) {
    count = 0U;
    nvs_handle_t handle{};
    auto error = nvs_open(kResetNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;

    error = nvs_get_u8(handle, kResetCountKey, &count);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        count = 0U;
        error = ESP_OK;
    }
    nvs_close(handle);
    return error;
}

esp_err_t save_reset_count(std::uint8_t count) {
    nvs_handle_t handle{};
    auto error = nvs_open(kResetNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;

    error = nvs_set_u8(handle, kResetCountKey, count);
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

esp_err_t clear_reset_count() {
    nvs_handle_t handle{};
    auto error = nvs_open(kResetNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;

    error = nvs_erase_key(handle, kResetCountKey);
    if (error == ESP_ERR_NVS_NOT_FOUND) error = ESP_OK;
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

void clear_sequence_after_window(void*) {
    vTaskDelay(kSequenceWindowTicks);
    const auto error = clear_reset_count();
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "Cannot clear reset sequence: %s", esp_err_to_name(error));
    } else {
        ESP_LOGI(kTag, "RST sequence window expired; counter cleared");
    }
    vTaskDelete(nullptr);
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
        ESP_LOGE(kTag, "Cannot start factory-reset erase worker; using synchronous erase");
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
            ESP_LOGW(kTag, "Factory-reset RGB step failed: %s", esp_err_to_name(rgb_error));
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
        (void)RgbDiagnostic::set_color(
            static_cast<int>(homeguard::board::kRgbLed),
            0xffU,
            0x00U,
            0x00U);
    }
}

}  // namespace

bool handle_triple_rst_factory_reset() {
    // HW-678 RST is wired to EN/CHIP_PU and is reported as POWERON, exactly like
    // a power cycle. Do not guess the reset source. Instead use a tiny persistent
    // counter with a short expiry window: three quick resets trigger Factory Reset.
    force_reset_rgb_off();

    std::uint8_t count = 0U;
    const auto load_error = load_reset_count(count);
    if (load_error != ESP_OK) {
        ESP_LOGE(kTag, "Cannot load reset counter: %s", esp_err_to_name(load_error));
        return false;
    }

    if (count < kRequiredPresses) ++count;
    const auto save_error = save_reset_count(count);
    if (save_error != ESP_OK) {
        ESP_LOGE(kTag, "Cannot save reset counter: %s", esp_err_to_name(save_error));
        return false;
    }

    ESP_LOGI(kTag, "RST sequence=%u/%u", static_cast<unsigned>(count), static_cast<unsigned>(kRequiredPresses));

    if (count < kRequiredPresses) {
        if (xTaskCreate(
                &clear_sequence_after_window,
                "hg_rst_window",
                1536,
                nullptr,
                3,
                nullptr) != pdPASS) {
            (void)clear_reset_count();
            ESP_LOGE(kTag, "Cannot arm RST expiry window; sequence cancelled");
        }
        return false;
    }

    (void)clear_reset_count();
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
