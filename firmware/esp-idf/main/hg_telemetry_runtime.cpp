#include "hg_telemetry_runtime.hpp"
#include "hg_hardware_bootstrap.hpp"

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdint>
#include <sstream>
#include <thread>

namespace homeguard::idf {

namespace {

constexpr const char* kTag = "hg_telemetry";
constexpr const char* kTelemetryPath = "/api/v1/telemetry/status";

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

esp_err_t TelemetryRuntime::register_handlers(httpd_handle_t server)
{
    if (server == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    const httpd_uri_t route{
        .uri = kTelemetryPath,
        .method = HTTP_GET,
        .handler = &TelemetryRuntime::status_get,
        .user_ctx = this,
    };
    return httpd_register_uri_handler(server, &route);
}

TelemetrySnapshot TelemetryRuntime::snapshot() const
{
    while (lock_.test_and_set(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    const auto copy = snapshot_;
    lock_.clear(std::memory_order_release);
    return copy;
}

void TelemetryRuntime::publish(const TelemetrySnapshot& snapshot)
{
    while (lock_.test_and_set(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    snapshot_ = snapshot;
    lock_.clear(std::memory_order_release);
}

void TelemetryRuntime::task_entry(void* context)
{
    static_cast<TelemetryRuntime*>(context)->run();
}

esp_err_t TelemetryRuntime::status_get(httpd_req_t* request)
{
    auto* runtime = static_cast<TelemetryRuntime*>(request->user_ctx);
    if (runtime == nullptr) {
        return httpd_resp_send_err(
            request,
            HTTPD_500_INTERNAL_SERVER_ERROR,
            "telemetry unavailable");
    }

    const auto current = runtime->snapshot();
    std::ostringstream out;
    out << "{\"sampledAtMs\":" << current.sampled_at_ms;

    out << ",\"temperatures\":[";
    if (current.rtc_temperature_valid) {
        out << "{\"index\":0,\"name\":\"Controller\",\"celsius\":"
            << current.rtc_temperature_c
            << ",\"state\":\"normal\"}";
    }
    out << ']';

    out << ",\"electrical\":[";
    if (current.battery_valid) {
        out << "{\"index\":0,\"name\":\"Battery\",\"voltage\":"
            << current.battery_voltage_v
            << ",\"current\":" << current.battery_current_a
            << ",\"power\":" << current.battery_power_w
            << ",\"state\":\"normal\"}";
    }
    out << "]}";

    const auto body = out.str();
    httpd_resp_set_type(request, "application/json");
    return httpd_resp_send(request, body.c_str(), body.size());
}

void TelemetryRuntime::run()
{
    while (true) {
        TelemetrySnapshot next{};
        next.sampled_at_ms = static_cast<std::uint64_t>(esp_timer_get_time() / 1000);

        Ina226Reading battery{};
        const auto battery_error =
            hardware_->battery_monitor().read(&battery);

        if (battery_error == ESP_OK) {
            next.battery_valid = true;
            next.battery_voltage_v = battery.bus_voltage_v;
            next.battery_current_a = battery.current_a;
            next.battery_power_w = battery.power_w;
            ESP_LOGI(
                kTag,
                "battery voltage=%.3f current=%.3f power=%.3f",
                static_cast<double>(battery.bus_voltage_v),
                static_cast<double>(battery.current_a),
                static_cast<double>(battery.power_w));
        }

        float rtc_temperature = 0.0F;
        const auto rtc_error = hardware_->rtc().read_temperature(&rtc_temperature);
        if (rtc_error == ESP_OK) {
            next.rtc_temperature_valid = true;
            next.rtc_temperature_c = rtc_temperature;
        }

        publish(next);
        hardware_->storage().refresh_space();

        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}

}  // namespace homeguard::idf
