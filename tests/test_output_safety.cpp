#include "test_framework.hpp"
#include "homeguard/output_command.hpp"
#include "homeguard/output_interlock.hpp"
#include "homeguard/physical_output_runtime.hpp"

#include <map>

namespace {
class FakeBackend final : public hg::PhysicalOutputBackend {
public:
    bool configure_output(int gpio, bool initial_level) override {
        levels[gpio] = initial_level;
        return !fail;
    }
    bool write_output(int gpio, bool level) override {
        if (fail) return false;
        levels[gpio] = level;
        return true;
    }
    bool fail{};
    std::map<int, bool> levels;
};

hg::HardwareVerificationRecord verified_hardware() {
    hg::HardwareVerificationRecord record{};
    record.pins.siren = 10;
    record.pins.valve1 = 11;
    record.pins.valve2 = 12;
    record.pins.aux1 = 13;
    record.pins.aux2 = 14;
    record.active_polarity_verified = true;
    record.verified_at_ms = 1;
    record.profile_crc32 = hg::hardware_profile_crc32(record);
    return record;
}

void test_interlock_policy() {
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

void test_output_commands() {
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

    result = hg::apply_output_command(model, ready, {1, true, false, 600});
    TEST_CHECK(result.status == hg::OutputCommandStatus::Applied);
    TEST_CHECK(model.output(1)->active);
    result = hg::apply_output_command(model, ready, {1, false, true, 700});
    TEST_CHECK(result.status == hg::OutputCommandStatus::Applied);
    TEST_CHECK(!model.output(1)->active);

    hg::BootReadinessReport after_reboot{};
    result = hg::apply_output_command(model, after_reboot, {1, true, false, 800});
    TEST_CHECK(result.status == hg::OutputCommandStatus::RejectedInterlock);
    TEST_CHECK(!model.output(1)->active);
}

void test_physical_runtime() {
    hg::SystemEventBus bus;
    hg::SystemModel model(bus);
    TEST_CHECK(model.add_output(1, hg::ModelOutputType::Siren));
    TEST_CHECK(model.add_output(2, hg::ModelOutputType::Valve));
    TEST_CHECK(model.add_output(3, hg::ModelOutputType::Valve));

    auto hardware = verified_hardware();
    hg::BootReadinessReport blocked{};
    FakeBackend backend;
    hg::PhysicalOutputRuntime runtime;
    TEST_CHECK(runtime.initialize(backend, hardware, blocked));
    TEST_CHECK(!runtime.state().outputs_enabled);
    TEST_CHECK(!backend.levels[10]);
    TEST_CHECK(!backend.levels[11]);
    TEST_CHECK(!backend.levels[12]);

    hg::BootReadinessReport ready{};
    ready.status = hg::BootReadinessStatus::ReadyForPhysicalOutputs;
    TEST_CHECK(model.set_output_active(1, true, 10));
    TEST_CHECK(model.set_output_active(2, true, 10));
    TEST_CHECK(model.set_output_active(3, true, 10));
    TEST_CHECK(runtime.synchronize(model, ready));
    TEST_CHECK(runtime.state().outputs_enabled);
    TEST_CHECK(backend.levels[10]);
    TEST_CHECK(backend.levels[11]);
    TEST_CHECK(backend.levels[12]);

    TEST_CHECK(model.set_output_active(3, false, 11));
    TEST_CHECK(runtime.synchronize(model, ready));
    TEST_CHECK(backend.levels[11]);
    TEST_CHECK(!backend.levels[12]);

    TEST_CHECK(runtime.synchronize(model, blocked));
    TEST_CHECK(!backend.levels[10]);
    TEST_CHECK(!backend.levels[11]);
    TEST_CHECK(!backend.levels[12]);
    TEST_CHECK(!runtime.state().outputs_enabled);

    FakeBackend failing;
    failing.fail = true;
    hg::PhysicalOutputRuntime broken;
    TEST_CHECK(!broken.initialize(failing, hardware, ready));
    TEST_CHECK(broken.state().status == hg::PhysicalOutputStatus::BackendError);
}
} // namespace

void test_output_safety() {
    test_interlock_policy();
    test_output_commands();
    test_physical_runtime();
}
