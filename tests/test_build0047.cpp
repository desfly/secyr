#include "test_framework.hpp"
#include "homeguard/commissioning_state.hpp"

#include <string>

void test_build0047() {
    hg::CommissioningPersistentState state{};
    CHECK(hg::CommissioningPersistentState::kSchemaVersion == 2U);
    CHECK(hg::validate_commissioning_state(state) == hg::CommissioningStateValidation::VerificationIncomplete);
    CHECK(hg::commissioning_state_persistable(state));
    CHECK(!hg::commissioning_state_allows_physical_outputs(state));

    // Hardware verification may be persisted before the first dry run. This is
    // safe progress, not permission to energize outputs.
    state.gpio_map_verified = true;
    state.active_polarity_verified = true;
    state.last_verified_at_ms = 100;
    CHECK(hg::validate_commissioning_state(state) == hg::CommissioningStateValidation::ValveSafetyUnverified);
    CHECK(hg::commissioning_state_persistable(state));
    CHECK(!hg::commissioning_state_allows_physical_outputs(state));

    // Dry-run progress is also persistable before the measured valve profile.
    state.successful_dry_runs = 1;
    state.last_verified_at_ms = 200;
    CHECK(hg::commissioning_state_persistable(state));
    CHECK(!hg::commissioning_state_allows_physical_outputs(state));

    // Partial valve profile is deliberately not persistable: all three measured
    // safety values are one atomic commissioning proof.
    auto partial_profile = state;
    partial_profile.valve_limit_polarity_verified = true;
    CHECK(!hg::commissioning_state_persistable(partial_profile));

    state.valve_limit_polarity_verified = true;
    state.valve_limits_active_low = true;
    state.cold_valve_travel_timeout_ms = 12345;
    state.hot_valve_travel_timeout_ms = 14567;
    state.last_verified_at_ms = 300;
    CHECK(hg::validate_commissioning_state(state) == hg::CommissioningStateValidation::Valid);
    CHECK(hg::commissioning_state_persistable(state));
    CHECK(!hg::commissioning_state_allows_physical_outputs(state));

    state.successful_actuator_tests = 1;
    state.last_verified_at_ms = 400;
    CHECK(hg::commissioning_state_persistable(state));
    CHECK(hg::commissioning_state_allows_physical_outputs(state));

    auto no_timeout = state;
    no_timeout.cold_valve_travel_timeout_ms = 0;
    CHECK(hg::validate_commissioning_state(no_timeout) == hg::CommissioningStateValidation::ValveSafetyUnverified);
    CHECK(!hg::commissioning_state_persistable(no_timeout));
    CHECK(!hg::commissioning_state_allows_physical_outputs(no_timeout));

    state.successful_actuator_tests = 2;
    CHECK(hg::validate_commissioning_state(state) == hg::CommissioningStateValidation::InvalidSequence);
    CHECK(!hg::commissioning_state_persistable(state));
    CHECK(!hg::commissioning_state_allows_physical_outputs(state));

    state.successful_actuator_tests = 1;
    state.schema_version = 1;
    CHECK(hg::validate_commissioning_state(state) == hg::CommissioningStateValidation::InvalidSchema);
    CHECK(!hg::commissioning_state_persistable(state));

    state.schema_version = hg::CommissioningPersistentState::kSchemaVersion;
    const std::string json = hg::commissioning_state_json(state);
    CHECK(json.find("\"gpioMapVerified\":true") != std::string::npos);
    CHECK(json.find("\"outputArchitecture\":\"mcp23017_port_a\"") != std::string::npos);
    CHECK(json.find("\"valveLimitPolarityVerified\":true") != std::string::npos);
    CHECK(json.find("\"coldValveTravelTimeoutMs\":12345") != std::string::npos);
    CHECK(json.find("\"persistable\":true") != std::string::npos);
    CHECK(json.find("\"physicalOutputsAllowed\":true") != std::string::npos);
}
