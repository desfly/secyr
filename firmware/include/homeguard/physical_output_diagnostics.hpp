#pragma once

#include "homeguard/boot_readiness.hpp"
#include "homeguard/physical_output_runtime.hpp"

#include <string>

namespace hg {

struct PhysicalOutputDiagnostics {
    PhysicalOutputStatus runtime_status{PhysicalOutputStatus::FailClosed};
    BootReadinessStatus boot_status{BootReadinessStatus::BlockedMissingHardwareRecord};
    bool outputs_enabled{};
    bool outputs_allowed{};
    std::uint32_t writes{};
    std::uint32_t failures{};
    bool healthy{};
};

[[nodiscard]] PhysicalOutputDiagnostics make_physical_output_diagnostics(
    const PhysicalOutputRuntimeState& runtime,
    const BootReadinessReport& readiness) noexcept;

[[nodiscard]] std::string physical_output_diagnostics_json(
    const PhysicalOutputDiagnostics& diagnostics);

}  // namespace hg
