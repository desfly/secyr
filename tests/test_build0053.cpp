#include "test_framework.hpp"
#include "homeguard/output_command.hpp"

void test_build0053(){
    hg::SystemEventBus bus;
    hg::SystemModel model(bus);
    TEST_CHECK(model.add_output(1, hg::ModelOutputType::Relay));

    hg::BootReadinessReport blocked{};
    auto r = hg::apply_output_command(model, blocked, {1,true,false,10});
    TEST_CHECK(r.status == hg::OutputCommandStatus::RejectedInterlock);
    TEST_CHECK(model.output(1) != nullptr && !model.output(1)->active);

    hg::BootReadinessReport ready{};
    ready.status = hg::BootReadinessStatus::ReadyForPhysicalOutputs;
    r = hg::apply_output_command(model, ready, {1,true,false,20});
    TEST_CHECK(r.status == hg::OutputCommandStatus::Applied);
    TEST_CHECK(model.output(1)->active);

    r = hg::apply_output_command(model, ready, {1,false,true,30});
    TEST_CHECK(r.status == hg::OutputCommandStatus::Applied);
    TEST_CHECK(!model.output(1)->active);

    // Simulated reboot/power loss: readiness returns fail-closed until persisted
    // commissioning is restored and validated again.
    hg::BootReadinessReport after_reboot{};
    r = hg::apply_output_command(model, after_reboot, {1,true,false,40});
    TEST_CHECK(r.status == hg::OutputCommandStatus::RejectedInterlock);
    TEST_CHECK(!model.output(1)->active);
}
