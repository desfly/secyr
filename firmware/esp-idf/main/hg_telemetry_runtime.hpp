#pragma once

#include "homeguard/telemetry.hpp"
#include "esp_err.h"

namespace hg {
class SystemModel;
}

class WebsocketTelemetry;

namespace homeguard::idf {

class HardwareBootstrap;

class TelemetryRuntime {
public:
    esp_err_t start(
        HardwareBootstrap* hardware,
        WebsocketTelemetry* websocket,
        const hg::SystemModel* system_model);

private:
    static void task_entry(void* context);
    void run();

    HardwareBootstrap* hardware_{nullptr};
    WebsocketTelemetry* websocket_{nullptr};
    const hg::SystemModel* system_model_{nullptr};
    hg::TelemetryBuilder builder_{};
    hg::HealthMonitor health_{};
};

}  // namespace homeguard::idf
