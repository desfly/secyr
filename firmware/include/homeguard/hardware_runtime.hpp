#pragma once

#include <cstdint>
#include <string>

namespace homeguard {

enum class HardwareModuleState {
    NotInitialized,
    Ready,
    Degraded,
    Missing,
    Fault,
};

struct HardwareModuleStatus {
    HardwareModuleState state{HardwareModuleState::NotInitialized};
    std::string detail;
    std::uint32_t error_count{0};
};

struct HardwareRuntimeStatus {
    HardwareModuleStatus i2c;
    HardwareModuleStatus ads1115_zones;
    HardwareModuleStatus ads1115_telemetry;
    HardwareModuleStatus mcp23017;
    HardwareModuleStatus ina226;
    HardwareModuleStatus ds3231;
    HardwareModuleStatus w5500;
    HardwareModuleStatus micro_sd;
    HardwareModuleStatus one_wire;
    HardwareModuleStatus rs485;
    // This is evidence, not an optimistic default. It becomes true only after
    // firmware has successfully driven the expander output latch to OFF.
    bool safe_outputs_forced{false};
};

const char* to_string(HardwareModuleState state) noexcept;
std::string hardware_runtime_json(const HardwareRuntimeStatus& status);

}  // namespace homeguard
