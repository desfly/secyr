#include "test_framework.hpp"
#include "homeguard/hardware_test.hpp"

void test_build0045() {
    hg::HardwareTestPolicy policy;
    hg::HardwareTestRequest request{hg::HardwareTestTarget::Siren, 250};

    {
        const auto result = policy.evaluate({}, request);
        CHECK(!result.allowed());
        CHECK(result.decision == hg::HardwareTestDecision::BlockedMaintenanceInactive);
    }
    {
        hg::HardwareTestContext context{true, true, false, true};
        const auto result = policy.evaluate(context, request);
        CHECK(result.decision == hg::HardwareTestDecision::BlockedSystemArmed);
    }
    {
        hg::HardwareTestContext context{true, false, true, true};
        const auto result = policy.evaluate(context, request);
        CHECK(result.decision == hg::HardwareTestDecision::BlockedAlarmActive);
    }
    {
        hg::HardwareTestContext context{true, false, false, false};
        const auto result = policy.evaluate(context, request);
        CHECK(result.decision == hg::HardwareTestDecision::BlockedOutputsUnavailable);
    }
    {
        hg::HardwareTestContext context{true, false, false, true};
        CHECK(policy.evaluate(context, {hg::HardwareTestTarget::Valve1, 0}).decision == hg::HardwareTestDecision::BlockedInvalidDuration);
        CHECK(policy.evaluate(context, {hg::HardwareTestTarget::Valve1, hg::HardwareTestPolicy::kMaxPulseMs + 1}).decision == hg::HardwareTestDecision::BlockedInvalidDuration);
        const auto allowed = policy.evaluate(context, {hg::HardwareTestTarget::Valve1, hg::HardwareTestPolicy::kMaxPulseMs});
        CHECK(allowed.allowed());
        CHECK(allowed.requested_outputs.valve1);
        CHECK(!allowed.requested_outputs.siren);
    }
    {
        const auto report = policy.readiness(true, true, true, true, false);
        CHECK(report.ready_for_dry_run());
        CHECK(!report.ready_for_actuator_test());
    }
    {
        const auto report = policy.readiness(true, true, true, true, true);
        CHECK(report.ready_for_dry_run());
        CHECK(report.ready_for_actuator_test());
    }
}
