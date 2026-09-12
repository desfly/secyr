#pragma once

#include "homeguard/telemetry.hpp"
#include "esp_err.h"

namespace hg {
class SystemModel;
}

class WebsocketTelemetry;

namespace homeguard::idf {

class HardwareBootstrap;
class BleTransport;

class TelemetryRuntime {
public:
    esp_err_t start(
        HardwareBootstrap* hardware,
        WebsocketTelemetry* websocket,
        hg::SystemModel* system_model,
        BleTransport* ble_transport = nullptr);

private:
    static void task_entry(void* context);
    void run();

    HardwareBootstrap* hardware_{nullptr};
    WebsocketTelemetry* websocket_{nullptr};
    hg::SystemModel* system_model_{nullptr};
    BleTransport* ble_transport_{nullptr};
    hg::TelemetryBuilder builder_{};
    hg::HealthMonitor health_{};
};

}  // namespace homeguard::idf
