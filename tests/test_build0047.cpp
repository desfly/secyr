#include "test_framework.hpp"
#include "homeguard/commissioning_state.hpp"

#include <string>

void test_build0047() {
    hg::CommissioningPersistentState state{};
    CHECK(hg::CommissioningPersistentState::kSchemaVersion == 2U);
    CHECK(hg::validate_commissioning_state(state) == hg::CommissioningStateValidation::VerificationIncomplete);
    CHECK(!hg::commissioning_state_allows_physical_outputs(state));

    state.gpio_map_verified = true;
    state.active_polarity_verified = true;
    CHECK(hg::validate_commissioning_state(state) == hg::CommissioningStateValidation::Valid);
    CHECK(!hg::commissioning_state_allows_physical_outputs(state));

    // A dry-run alone is deliberately insufficient for the MCP actuator path.
    state.successful_dry_runs = 1;
    state.last_verified_at_ms = 123456;
    CHECK(!hg::commissioning_state_allows_physical_outputs(state));

    state.successful_actuator_tests = 1;
    CHECK(hg::commissioning_state_allows_physical_outputs(state));

    state.successful_actuator_tests = 2;
    CHECK(hg::validate_commissioning_state(state) == hg::CommissioningStateValidation::InvalidSequence);
    CHECK(!hg::commissioning_state_allows_physical_outputs(state));

    state.successful_actuator_tests = 1;
    state.schema_version = 1;
    CHECK(hg::validate_commissioning_state(state) == hg::CommissioningStateValidation::InvalidSchema);

    state.schema_version = hg::CommissioningPersistentState::kSchemaVersion;
    const std::string json = hg::commissioning_state_json(state);
    CHECK(json.find("\"gpioMapVerified\":true") != std::string::npos);
    CHECK(json.find("\"outputArchitecture\":\"mcp23017_port_a\"") != std::string::npos);
    CHECK(json.find("\"physicalOutputsAllowed\":true") != std::string::npos);
}
