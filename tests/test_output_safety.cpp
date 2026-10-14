#include "test_framework.hpp"
#include "homeguard/output_command.hpp"
#include "homeguard/physical_output_runtime.hpp"

#include <map>

namespace {
class FakeBackend final : public hg::PhysicalOutputBackend {
public:
    bool configure_output(int gpio, bool initial) override { levels[gpio] = initial; return !fail; }
    bool write_output(int gpio, bool level) override { if (fail) return false; levels[gpio] = level; return true; }
    bool fail{};
    std::map<int, bool> levels;
};

hg::HardwareVerificationRecord verified_hardware() {
    hg::HardwareVerificationRecord record{};
    record.pins.siren = 10;
    record.pins.valve1 = 11;
    record.pins.valve2 = 12;
    record.active_polarity_verified = true;
    record.verified_at_ms = 1;
    record.profile_crc32 = hg::hardware_profile_crc32(record);
    return record;
}

void test_interlocks() {
    hg::SystemEventBus bus;
    hg::SystemModel model(bus);
    TEST_CHECK(model.add_output(1, hg::ModelOutputType::Siren));
    TEST_CHECK(model.add_output(4, hg::ModelOutputType::Light));
    TEST_CHECK(model.add_output(5, hg::ModelOutputType::Relay));

    hg::BootReadinessReport blocked{};
    auto result = hg::apply_output_command(model, blocked, {1, true, false, 1});
    TEST_CHECK(result.status == hg::OutputCommandStatus::RejectedInterlock);

    result = hg::apply_output_command(model, blocked, {4, true, false, 2});
    TEST_CHECK(result.status == hg::OutputCommandStatus::Applied);
    TEST_CHECK(model.output(4)->active);

    result = hg::apply_output_command(model, blocked, {5, true, false, 3});
    TEST_CHECK(result.status == hg::OutputCommandStatus::Applied);
    TEST_CHECK(model.output(5)->active);

    result = hg::apply_output_command(model, blocked, {4, true, true, 4});
    TEST_CHECK(result.status == hg::OutputCommandStatus::RejectedInterlock);
}

void test_runtime() {
    hg::SystemEventBus bus;
    hg::SystemModel model(bus);
    TEST_CHECK(model.add_output(1, hg::ModelOutputType::Siren));
    TEST_CHECK(model.add_output(2, hg::ModelOutputType::Valve));
    TEST_CHECK(model.add_output(3, hg::ModelOutputType::Valve));
    TEST_CHECK(model.add_output(4, hg::ModelOutputType::Light));
    TEST_CHECK(model.add_output(5, hg::ModelOutputType::Relay));

    auto hardware = verified_hardware();
    hg::BootReadinessReport blocked{};
    FakeBackend backend;
    hg::PhysicalOutputRuntime runtime;
    TEST_CHECK(runtime.initialize(backend, hardware, blocked));

    TEST_CHECK(model.set_output_active(1, true, 1));
    TEST_CHECK(model.set_output_active(2, true, 1));
    TEST_CHECK(model.set_output_active(3, true, 1));
    TEST_CHECK(model.set_output_active(4, true, 1));
    TEST_CHECK(model.set_output_active(5, true, 1));
    TEST_CHECK(runtime.synchronize(model, blocked));

    TEST_CHECK(!backend.levels[10]);
    TEST_CHECK(!backend.levels[11]);
    TEST_CHECK(!backend.levels[12]);
    TEST_CHECK(backend.levels[hg::direct_light_relay_gpio]);
    TEST_CHECK(backend.levels[hg::direct_lock_relay_gpio]);
    // Virtual MCP23017 IDs must not silently touch ESP32 GPIO1/GPIO2.
    TEST_CHECK(backend.levels.find(1) == backend.levels.end());
    TEST_CHECK(backend.levels.find(2) == backend.levels.end());

    hg::BootReadinessReport ready{};
    ready.status = hg::BootReadinessStatus::ReadyForPhysicalOutputs;
    TEST_CHECK(runtime.synchronize(model, ready));
    TEST_CHECK(backend.levels[10]);
    TEST_CHECK(backend.levels[11]);
    TEST_CHECK(backend.levels[12]);

    // Emergency shutdown must clear both MCP virtual outputs and all
    // commissioned physical outputs even after they were energized.
    TEST_CHECK(runtime.force_safe());
    TEST_CHECK(!backend.levels[hg::direct_light_relay_gpio]);
    TEST_CHECK(!backend.levels[hg::direct_lock_relay_gpio]);
    TEST_CHECK(!backend.levels[10]);
    TEST_CHECK(!backend.levels[11]);
    TEST_CHECK(!backend.levels[12]);
    TEST_CHECK(!runtime.state().outputs_enabled);

    FakeBackend failing;
    failing.fail = true;
    hg::PhysicalOutputRuntime broken;
    TEST_CHECK(!broken.initialize(failing, hardware, ready));
    TEST_CHECK(broken.state().status == hg::PhysicalOutputStatus::BackendError);

    // A runtime write failure must never be reported as a successful sync.
    FakeBackend write_failure;
    hg::PhysicalOutputRuntime guarded;
    TEST_CHECK(guarded.initialize(write_failure, hardware, ready));
    write_failure.fail = true;
    TEST_CHECK(!guarded.synchronize(model, ready));
    TEST_CHECK(guarded.state().status == hg::PhysicalOutputStatus::BackendError);
    TEST_CHECK(!guarded.state().outputs_enabled);
}
} // namespace

void test_output_safety() {
    test_interlocks();
    test_runtime();
}
