#include "test_framework.hpp"
#include "homeguard/output_interlock.hpp"

void test_build0051()
{
    hg::SystemEventBus bus;
    hg::SystemModel model(bus);
    testfw::require(model.add_output(1, hg::ModelOutputType::Siren), "add output");

    hg::BootReadinessReport blocked{};
    auto result = hg::evaluate_output_interlock(model, {1, true, false, &blocked});
    testfw::require(!result.allow, "blocked boot must reject activation");
    testfw::require(result.decision == hg::OutputInterlockDecision::BootNotReady, "boot reason");

    hg::BootReadinessReport ready{};
    ready.status = hg::BootReadinessStatus::ReadyForPhysicalOutputs;
    result = hg::evaluate_output_interlock(model, {1, true, false, &ready});
    testfw::require(result.allow, "ready boot allows activation");

    result = hg::evaluate_output_interlock(model, {1, true, true, &ready});
    testfw::require(!result.allow, "alarm ownership blocks manual activation");
    testfw::require(result.decision == hg::OutputInterlockDecision::AlarmActive, "alarm reason");

    result = hg::evaluate_output_interlock(model, {1, false, true, &ready});
    testfw::require(result.allow, "safe deactivation remains allowed");

    result = hg::evaluate_output_interlock(model, {99, true, false, &ready});
    testfw::require(!result.allow, "unknown output rejected");
    testfw::require(result.decision == hg::OutputInterlockDecision::InvalidOutput, "invalid output reason");
}
