#include "homeguard/hardware_runtime.hpp"

#include <sstream>

namespace homeguard {

const char* to_string(HardwareModuleState state) noexcept
{
    switch (state) {
    case HardwareModuleState::NotInitialized: return "not_initialized";
    case HardwareModuleState::Ready: return "ready";
    case HardwareModuleState::Degraded: return "degraded";
    case HardwareModuleState::Missing: return "missing";
    case HardwareModuleState::Fault:
    default: return "fault";
    }
}

const char* to_string(HardwareBootstrapState state) noexcept
{
    switch (state) {
    case HardwareBootstrapState::NotInitialized: return "not_initialized";
    case HardwareBootstrapState::Ready: return "ready";
    case HardwareBootstrapState::Degraded: return "degraded";
    case HardwareBootstrapState::Failed:
    default: return "failed";
    }
}

namespace {

void module_json(
    std::ostringstream& output,
    const char* name,
    const HardwareModuleStatus& module,
    bool comma)
{
    output << "\"" << name << "\":{"
           << "\"state\":\"" << to_string(module.state) << "\","
           << "\"detail\":\"" << module.detail << "\","
           << "\"error_count\":" << module.error_count
           << "}";
    if (comma) output << ",";
}

}  // namespace

std::string hardware_runtime_json(const HardwareRuntimeStatus& status)
{
    std::ostringstream output;
    output << "{\"overall\":\"" << to_string(status.overall) << "\",";
    module_json(output, "i2c", status.i2c, true);
    module_json(output, "ads1115_zones", status.ads1115_zones, true);
    module_json(output, "ads1115_telemetry", status.ads1115_telemetry, true);
    module_json(output, "mcp23017", status.mcp23017, true);
    module_json(output, "ina226", status.ina226, true);
    module_json(output, "ds3231", status.ds3231, true);
    module_json(output, "w5500", status.w5500, true);
    module_json(output, "micro_sd", status.micro_sd, true);
    module_json(output, "one_wire", status.one_wire, true);
    module_json(output, "rs485", status.rs485, true);
    output << "\"safe_outputs_forced\":"
           << (status.safe_outputs_forced ? "true" : "false")
           << "}";
    return output.str();
}

}  // namespace homeguard
