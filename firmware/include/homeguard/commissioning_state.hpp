#pragma once

#include <cstdint>
#include <string>

namespace hg {

struct CommissioningPersistentState {
    static constexpr std::uint32_t kSchemaVersion = 2;

    std::uint32_t schema_version{kSchemaVersion};
    bool gpio_map_verified{};
    bool active_polarity_verified{};
    std::uint32_t successful_dry_runs{};
    std::uint32_t successful_actuator_tests{};
    std::uint64_t last_verified_at_ms{};

    // Valve safety is measured during commissioning, never guessed by firmware.
    bool valve_limit_polarity_verified{};
    bool valve_limits_active_low{};
    std::uint32_t cold_valve_travel_timeout_ms{};
    std::uint32_t hot_valve_travel_timeout_ms{};
};

enum class CommissioningStateValidation {
    Valid,
    InvalidSchema,
    InvalidSequence,
    VerificationIncomplete,
    ValveSafetyUnverified,
};

CommissioningStateValidation validate_commissioning_state(
    const CommissioningPersistentState& state) noexcept;

// Persistable is intentionally weaker than Valid. It permits safe, monotonic
// commissioning progress (for example hardware+dry-run completed but valve
// profile not measured yet) to survive reboot. Persistable never implies ON.
bool commissioning_state_persistable(
    const CommissioningPersistentState& state) noexcept;

bool commissioning_state_allows_physical_outputs(
    const CommissioningPersistentState& state) noexcept;

std::string commissioning_state_json(const CommissioningPersistentState& state);
const char* to_string(CommissioningStateValidation validation) noexcept;

}  // namespace hg
