#include "test_framework.hpp"
#include "homeguard/output_command.hpp"

void test_build0052()
{
    hg::SystemEventBus bus;
    hg::SystemModel model(bus);
    testfw::expect(model.add_output(1, hg::ModelOutputType::Relay), "output add");

    hg::BootReadinessReport blocked{};
    auto result = hg::apply_output_command(model, blocked, {1, true, false, 100});
    testfw::expect(result.status == hg::OutputCommandStatus::RejectedInterlock, "blocked activation rejected");
    testfw::expect(!model.output(1)->active, "blocked output remains off");

    hg::BootReadinessReport ready{};
    ready.status = hg::BootReadinessStatus::ReadyForPhysicalOutputs;
    result = hg::apply_output_command(model, ready, {1, true, false, 200});
    testfw::expect(result.status == hg::OutputCommandStatus::Applied, "ready activation applied");
    testfw::expect(model.output(1)->active, "output active after allowed command");

    result = hg::apply_output_command(model, ready, {1, true, true, 300});
    testfw::expect(result.status == hg::OutputCommandStatus::RejectedInterlock, "alarm ownership blocks manual activation");

    blocked.status = hg::BootReadinessStatus::BlockedDryRunRequired;
    result = hg::apply_output_command(model, blocked, {1, false, true, 400});
    testfw::expect(result.status == hg::OutputCommandStatus::Applied, "safe deactivation always allowed");
    testfw::expect(!model.output(1)->active, "safe deactivation turns output off");

    result = hg::apply_output_command(model, ready, {99, true, false, 500});
    testfw::expect(result.status == hg::OutputCommandStatus::InvalidOutput, "unknown output rejected");
}
