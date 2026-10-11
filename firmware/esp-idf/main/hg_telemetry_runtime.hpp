#pragma once

#include "homeguard/telemetry.hpp"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"

#include <array>
#include <cstdint>

namespace hg {
class SystemModel;
class SystemEventBus;
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
        hg::SystemEventBus* system_bus,
        BleTransport* ble_transport = nullptr);

    [[nodiscard]] bool mcp_outputs_healthy() const noexcept { return mcp_outputs_healthy_; }
    [[nodiscard]] std::uint8_t mcp_outputs_applied() const noexcept { return mcp_outputs_applied_; }

private:
    static void task_entry(void* context);
    static void zone_task_entry(void* context);
    static void output_mirror_task_entry(void* context);
    void run_output_mirror();
    void run();
    void run_zones();
    void update_zone_model(const std::array<hg::ZoneState, 8>& zones, std::uint64_t now_ms);
    void update_zone_light(const std::array<hg::ZoneState, 8>& zones, std::uint64_t now_ms);
    bool set_light_output(bool active, std::uint64_t now_ms);

    std::array<hg::ZoneState, 8> zone_snapshot_{};
    portMUX_TYPE zone_snapshot_lock_ = portMUX_INITIALIZER_UNLOCKED;

    HardwareBootstrap* hardware_{nullptr};
    WebsocketTelemetry* websocket_{nullptr};
    hg::SystemModel* system_model_{nullptr};
    hg::SystemEventBus* system_bus_{nullptr};
    BleTransport* ble_transport_{nullptr};
    hg::TelemetryBuilder builder_{};
    hg::HealthMonitor health_{};

    bool light_cycle_active_{false};
    bool light_restore_active_{false};
    std::uint64_t light_cycle_deadline_ms_{0};
    std::uint64_t next_telemetry_ms_{0};
    volatile bool mcp_outputs_healthy_{false};
    volatile std::uint8_t mcp_outputs_applied_{0};
};

}  // namespace homeguard::idf
