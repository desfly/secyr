#include "hg_telemetry_runtime.hpp"
#include "hg_hardware_bootstrap.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace homeguard::idf {

namespace {

constexpr const char* kTag = "hg_telemetry";

}  // namespace

esp_err_t TelemetryRuntime::start(
    HardwareBootstrap* hardware)
{
    if (hardware == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    hardware_ = hardware;

    const auto result = xTaskCreate(
        &TelemetryRuntime::task_entry,
        "hg_telemetry",
        6144,
        this,
        6,
        nullptr);

    return result == pdPASS ?
        ESP_OK :
        ESP_ERR_NO_MEM;
}

void TelemetryRuntime::task_entry(void* context)
{
    static_cast<TelemetryRuntime*>(context)->run();
}

void TelemetryRuntime::run()
{
    while (true) {
        Ina226Reading battery{};
        const auto battery_error =
            hardware_->battery_monitor().read(&battery);

        if (battery_error == ESP_OK) {
            ESP_LOGI(
                kTag,
                "battery voltage=%.3f current=%.3f power=%.3f",
                static_cast<double>(battery.bus_voltage_v),
                static_cast<double>(battery.current_a),
                static_cast<double>(battery.power_w));
        }

        float rtc_temperature = 0.0F;
        hardware_->rtc().read_temperature(
            &rtc_temperature);

        hardware_->storage().refresh_space();

        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

}  // namespace homeguard::idf
