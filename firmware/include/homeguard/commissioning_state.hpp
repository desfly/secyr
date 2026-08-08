#pragma once

#include <cstdint>
#include <string>

namespace hg {

struct CommissioningPersistentState {
    static constexpr std::uint32_t kSchemaVersion = 1;

    std::uint32_t schema_version{kSchemaVersion};
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
