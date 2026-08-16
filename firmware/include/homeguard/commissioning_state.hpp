#pragma once

#include <cstdint>
#include <string>

namespace hg {

struct CommissioningPersistentState {
    // Schema 2 belongs to the HW-678 MCP23017 actuator architecture. Schema 1
    // dry-runs/actuator-tests were performed against the legacy direct-GPIO
    // model and must never unlock schema-2 physical outputs.
    static constexpr std::uint32_t kSchemaVersion = 2;

    std::uint32_t schema_version{kSchemaVersion};
    // Historical field name retained in the persisted structure for compact
    // source compatibility. In schema 2 it means the fixed HW-678 hardware and
    // MCP23017 output allocation were verified.
    bool gpio_map_verified{};
    bool active_polarity_verified{};
    std::uint32_t successful_dry_runs{};
    std::uint32_t successful_actuator_tests{};
    std::uint64_t last_verified_at_ms{};

    // Valve safety is measured during commissioning, never guessed by firmware.
    // Zero timeout or unknown limit polarity keeps the physical output gate
    // closed. Each valve keeps its own measured maximum travel timeout.
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

bool commissioning_state_allows_physical_outputs(
    const CommissioningPersistentState& state) noexcept;

std::string commissioning_state_json(const CommissioningPersistentState& state);
const char* to_string(CommissioningStateValidation validation) noexcept;

}  // namespace hg
