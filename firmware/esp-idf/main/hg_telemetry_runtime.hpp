#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"

#include <atomic>
#include <cstdint>

namespace homeguard::idf {

class HardwareBootstrap;

struct TelemetrySnapshot {
    std::uint64_t sampled_at_ms{0};
    bool battery_valid{false};
    float battery_voltage_v{0.0F};
    float battery_current_a{0.0F};
    float battery_power_w{0.0F};
    bool rtc_temperature_valid{false};
    float rtc_temperature_c{0.0F};
};

class TelemetryRuntime {
public:
    esp_err_t start(HardwareBootstrap* hardware);
    esp_err_t register_handlers(httpd_handle_t server);
    TelemetrySnapshot snapshot() const;

private:
    static void task_entry(void* context);
    static esp_err_t status_get(httpd_req_t* request);
    void run();
    void publish(const TelemetrySnapshot& snapshot);

    HardwareBootstrap* hardware_{nullptr};
    mutable std::atomic_flag lock_ = ATOMIC_FLAG_INIT;
    TelemetrySnapshot snapshot_{};
};

}  // namespace homeguard::idf
