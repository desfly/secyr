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
    if (!state.valve_limit_polarity_verified ||
        state.cold_valve_travel_timeout_ms == 0U ||
        state.hot_valve_travel_timeout_ms == 0U) {
        return CommissioningStateValidation::ValveSafetyUnverified;
    }
    return CommissioningStateValidation::Valid;
}

bool commissioning_state_persistable(
    const CommissioningPersistentState& state) noexcept
{
    if (state.schema_version != CommissioningPersistentState::kSchemaVersion) return false;
    if (state.successful_actuator_tests > state.successful_dry_runs) return false;

    const bool has_progress =
        state.gpio_map_verified || state.active_polarity_verified ||
        state.successful_dry_runs != 0U || state.successful_actuator_tests != 0U ||
        state.valve_limit_polarity_verified ||
        state.cold_valve_travel_timeout_ms != 0U ||
        state.hot_valve_travel_timeout_ms != 0U;
    if (has_progress && state.last_verified_at_ms == 0U) return false;

    if (state.successful_dry_runs != 0U &&
        (!state.gpio_map_verified || !state.active_polarity_verified)) {
        return false;
    }

    const bool any_valve_profile =
        state.valve_limit_polarity_verified ||
        state.cold_valve_travel_timeout_ms != 0U ||
        state.hot_valve_travel_timeout_ms != 0U;
    const bool complete_valve_profile =
        state.valve_limit_polarity_verified &&
        state.cold_valve_travel_timeout_ms != 0U &&
        state.hot_valve_travel_timeout_ms != 0U;
    if (any_valve_profile && !complete_valve_profile) return false;

    if (state.successful_actuator_tests != 0U &&
        (!state.gpio_map_verified || !state.active_polarity_verified ||
         !complete_valve_profile)) {
        return false;
    }

    return true;
}

bool commissioning_state_allows_physical_outputs(
    const CommissioningPersistentState& state) noexcept
{
    return validate_commissioning_state(state) == CommissioningStateValidation::Valid &&
           state.successful_dry_runs > 0U &&
           state.successful_actuator_tests > 0U;
}

std::string commissioning_state_json(const CommissioningPersistentState& state)
{
    std::ostringstream out;
    out << "{\"schemaVersion\":" << state.schema_version
        << ",\"outputArchitecture\":\"mcp23017_port_a\""
        << ",\"gpioMapVerified\":" << (state.gpio_map_verified ? "true" : "false")
        << ",\"activePolarityVerified\":" << (state.active_polarity_verified ? "true" : "false")
        << ",\"successfulDryRuns\":" << state.successful_dry_runs
        << ",\"successfulActuatorTests\":" << state.successful_actuator_tests
        << ",\"lastVerifiedAtMs\":" << state.last_verified_at_ms
        << ",\"valveLimitPolarityVerified\":" << (state.valve_limit_polarity_verified ? "true" : "false")
        << ",\"valveLimitsActiveLow\":" << (state.valve_limits_active_low ? "true" : "false")
        << ",\"coldValveTravelTimeoutMs\":" << state.cold_valve_travel_timeout_ms
        << ",\"hotValveTravelTimeoutMs\":" << state.hot_valve_travel_timeout_ms
        << ",\"persistable\":" << (commissioning_state_persistable(state) ? "true" : "false")
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
    case CommissioningStateValidation::ValveSafetyUnverified: return "valve_safety_unverified";
    }
    return "unknown";
}

}  // namespace hg
