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
    void update_zone_light(const std::array<hg::ZoneState, 8>& zones, std::uint64_t now_ms);
    bool set_light_output(bool active, std::uint64_t now_ms);

    HardwareBootstrap* hardware_{nullptr};
    WebsocketTelemetry* websocket_{nullptr};
    hg::SystemModel* system_model_{nullptr};
    BleTransport* ble_transport_{nullptr};
    hg::TelemetryBuilder builder_{};
    hg::HealthMonitor health_{};

    bool light_cycle_active_{false};
    bool light_restore_active_{false};
    std::uint64_t light_cycle_deadline_ms_{0};
};

}  // namespace homeguard::idf
