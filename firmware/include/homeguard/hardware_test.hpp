#pragma once

#include "homeguard/maintenance.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace hg {

enum class HardwareTestTarget {
    Siren,
    Valve1,
    Valve2,
    Aux1,
    Aux2,
};

enum class HardwareTestDecision {
    Allowed,
    BlockedMaintenanceInactive,
    BlockedSystemArmed,
    BlockedAlarmActive,
    BlockedOutputsUnavailable,
    BlockedInvalidDuration,
};

struct HardwareTestContext {
    bool maintenance_active{};
    bool system_armed{};
    bool alarm_active{};
    bool physical_outputs_available{};
};

struct HardwareTestRequest {
    HardwareTestTarget target{HardwareTestTarget::Siren};
    std::uint32_t duration_ms{};
};

struct HardwareTestResult {
    HardwareTestDecision decision{HardwareTestDecision::BlockedOutputsUnavailable};
    HardwareTestTarget target{HardwareTestTarget::Siren};
    std::uint32_t requested_duration_ms{};
    Outputs requested_outputs{};

    [[nodiscard]] bool allowed() const noexcept { return decision == HardwareTestDecision::Allowed; }
};

struct HardwareReadinessItem {
    std::string name;
    bool ready{};
    std::string detail;
};

struct HardwareReadinessReport {
    std::vector<HardwareReadinessItem> items;

    [[nodiscard]] bool ready_for_dry_run() const noexcept;
    [[nodiscard]] bool ready_for_actuator_test() const noexcept;
};

class HardwareTestPolicy {
public:
    static constexpr std::uint32_t kMaxPulseMs = 1000;

    [[nodiscard]] HardwareTestResult evaluate(
        const HardwareTestContext& context,
        const HardwareTestRequest& request) const noexcept;

    [[nodiscard]] HardwareReadinessReport readiness(
        bool controller_alive,
        bool zones_available,
        bool analog_available,
        bool event_log_available,
        bool physical_outputs_available) const;

    [[nodiscard]] static Outputs output_for(HardwareTestTarget target) noexcept;
};

const char* to_string(HardwareTestTarget target) noexcept;
const char* to_string(HardwareTestDecision decision) noexcept;

}  // namespace hg
