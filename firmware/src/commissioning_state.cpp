#include "homeguard/commissioning_state.hpp"

#include <sstream>

namespace hg {

CommissioningStateValidation validate_commissioning_state(
    const CommissioningPersistentState& state) noexcept
{
    if (state.schema_version != CommissioningPersistentState::kSchemaVersion) {
        return CommissioningStateValidation::InvalidSchema;
    }
    if (state.successful_actuator_tests > state.successful_dry_runs) {
        return CommissioningStateValidation::InvalidSequence;
    }
    if (!state.gpio_map_verified || !state.active_polarity_verified) {
        return CommissioningStateValidation::VerificationIncomplete;
    }
    return CommissioningStateValidation::Valid;
}

bool commissioning_state_allows_physical_outputs(
    const CommissioningPersistentState& state) noexcept
{
    return validate_commissioning_state(state) == CommissioningStateValidation::Valid &&
           state.successful_dry_runs > 0;
}

std::string commissioning_state_json(const CommissioningPersistentState& state)
{
    std::ostringstream out;
    out << "{\"schemaVersion\":" << state.schema_version
        << ",\"gpioMapVerified\":" << (state.gpio_map_verified ? "true" : "false")
        << ",\"activePolarityVerified\":" << (state.active_polarity_verified ? "true" : "false")
        << ",\"successfulDryRuns\":" << state.successful_dry_runs
        << ",\"successfulActuatorTests\":" << state.successful_actuator_tests
        << ",\"lastVerifiedAtMs\":" << state.last_verified_at_ms
        << ",\"validation\":\"" << to_string(validate_commissioning_state(state)) << "\""
        << ",\"physicalOutputsAllowed\":" << (commissioning_state_allows_physical_outputs(state) ? "true" : "false")
        << "}";
    return out.str();
}

const char* to_string(CommissioningStateValidation validation) noexcept
{
    switch (validation) {
    case CommissioningStateValidation::Valid: return "valid";
    case CommissioningStateValidation::InvalidSchema: return "invalid_schema";
    case CommissioningStateValidation::InvalidSequence: return "invalid_sequence";
    case CommissioningStateValidation::VerificationIncomplete: return "verification_incomplete";
    }
    return "unknown";
}

}  // namespace hg
