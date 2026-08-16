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
    // compatibility of source code. In schema 2 it means the fixed HW-678
    // hardware/output map (including MCP23017 allocation) was verified.
    bool gpio_map_verified{};
    bool active_polarity_verified{};
    std::uint32_t successful_dry_runs{};
    std::uint32_t successful_actuator_tests{};
    std::uint64_t last_verified_at_ms{};
};

enum class CommissioningStateValidation {
    Valid,
    InvalidSchema,
    InvalidSequence,
    VerificationIncomplete,
};

CommissioningStateValidation validate_commissioning_state(
    const CommissioningPersistentState& state) noexcept;

bool commissioning_state_allows_physical_outputs(
    const CommissioningPersistentState& state) noexcept;

std::string commissioning_state_json(const CommissioningPersistentState& state);
const char* to_string(CommissioningStateValidation validation) noexcept;

}  // namespace hg
