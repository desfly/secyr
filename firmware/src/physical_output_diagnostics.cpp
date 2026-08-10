#include "homeguard/physical_output_diagnostics.hpp"

#include <sstream>

namespace hg {

PhysicalOutputDiagnostics make_physical_output_diagnostics(
    const PhysicalOutputRuntimeState& runtime,
    const BootReadinessReport& readiness) noexcept
{
    PhysicalOutputDiagnostics result{};
    result.runtime_status = runtime.status;
    result.boot_status = readiness.status;
    result.outputs_enabled = runtime.outputs_enabled;
    result.outputs_allowed = readiness.outputs_allowed();
    result.writes = runtime.writes;
    result.failures = runtime.failures;
    result.healthy = runtime.failures == 0 &&
        runtime.status != PhysicalOutputStatus::BackendError &&
        runtime.status != PhysicalOutputStatus::InvalidHardware &&
        (!result.outputs_enabled || result.outputs_allowed);
    return result;
}

std::string physical_output_diagnostics_json(const PhysicalOutputDiagnostics& d)
{
    std::ostringstream out;
    out << "{\"runtimeStatus\":\"" << to_string(d.runtime_status)
        << "\",\"bootStatus\":\"" << to_string(d.boot_status)
        << "\",\"outputsEnabled\":" << (d.outputs_enabled ? "true" : "false")
        << ",\"outputsAllowed\":" << (d.outputs_allowed ? "true" : "false")
        << ",\"writes\":" << d.writes
        << ",\"failures\":" << d.failures
        << ",\"healthy\":" << (d.healthy ? "true" : "false")
        << '}';
    return out.str();
}

}  // namespace hg
