#include "hg_reset_sequence.hpp"

#include "hg_board_hw678.hpp"
#include "hg_factory_reset.hpp"
#include "hg_rgb_diagnostic.hpp"

#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs.h"

#include <cstdint>

namespace homeguard::idf {
namespace {

constexpr const char* kTag = "hg_rst_sequence";
constexpr const char* kResetNamespace = "hg_rst";
constexpr const char* kResetCountKey = "count";
constexpr std::uint8_t kRequiredPresses = 3U;
constexpr std::uint64_t kSequenceWindowUs = 4500000ULL;
constexpr unsigned kFactoryResetWhiteMs = 5000U;

esp_timer_handle_t g_reset_clear_timer = nullptr;

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

void reset_window_expired(void*) {
    const auto error = clear_reset_count();
    if (error != ESP_OK) ESP_LOGE(kTag, "Cannot clear reset sequence: %s", esp_err_to_name(error));
    else ESP_LOGI(kTag, "RST sequence window expired; counter cleared");
}

esp_err_t arm_reset_window() {
    if (g_reset_clear_timer == nullptr) {
        const esp_timer_create_args_t args{
            .callback = &reset_window_expired,
            .arg = nullptr,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "hg_rst_window",
            .skip_unhandled_events = true,
        };
        const auto create_error = esp_timer_create(&args, &g_reset_clear_timer);
        if (create_error != ESP_OK) return create_error;
    }
    if (esp_timer_is_active(g_reset_clear_timer)) {
        const auto stop_error = esp_timer_stop(g_reset_clear_timer);
        if (stop_error != ESP_OK) return stop_error;
    }
    return esp_timer_start_once(g_reset_clear_timer, kSequenceWindowUs);
}

void force_reset_rgb_off() {
    const auto error = RgbDiagnostic::off(static_cast<int>(homeguard::board::kRgbLed));
    if (error != ESP_OK) ESP_LOGW(kTag, "Cannot force reset RGB off: %s", esp_err_to_name(error));
}

bool is_button_style_reset(esp_reset_reason_t reason) {
    // HW-678 EN/RST is reported as POWERON. EXT is accepted for compatibility.
    // Software, watchdog, panic and brownout resets must never advance the counter.
    return reason == ESP_RST_POWERON || reason == ESP_RST_EXT;
}

}  // namespace

bool handle_triple_rst_factory_reset() {
    force_reset_rgb_off();

    const auto reason = esp_reset_reason();
    if (!is_button_style_reset(reason)) {
        (void)clear_reset_count();
        ESP_LOGI(kTag, "Reset reason=%d ignored; RST sequence cleared", static_cast<int>(reason));
        return false;
    }

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
        const auto timer_error = arm_reset_window();
        if (timer_error != ESP_OK) {
            (void)clear_reset_count();
            ESP_LOGE(kTag, "Cannot arm RST expiry timer; sequence cancelled: %s", esp_err_to_name(timer_error));
        }
        return false;
    }

    (void)clear_reset_count();
    ESP_LOGW(kTag, "Triple RST detected: full Factory Reset accepted");

    const auto white_error = RgbDiagnostic::test_white(
        static_cast<int>(homeguard::board::kRgbLed), kFactoryResetWhiteMs);
    if (white_error != ESP_OK) {
        ESP_LOGW(kTag, "Factory-reset white confirmation failed: %s", esp_err_to_name(white_error));
    }

    ESP_LOGW(kTag, "Factory Reset: erasing all mutable HomeGuard state");
    const auto report = FactoryResetManager{}.erase_mutable_state();
    if (!report.ok()) {
        force_reset_rgb_off();
        ESP_LOGE(kTag,
                 "Full Factory Reset failed: access=%d wifi=%d cloud=%d config=%d provisioning=%d commissioning=%d",
                 report.access,
                 report.wifi,
                 report.cloud,
                 report.controller_config,
                 report.provisioning,
                 report.commissioning);
        return true;
    }

    force_reset_rgb_off();
    ESP_LOGW(kTag, "Full Factory Reset complete; rebooting clean");
    esp_restart();
    return true;
}

}  // namespace homeguard::idf
