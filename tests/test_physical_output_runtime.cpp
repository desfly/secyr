#include "test_framework.hpp"
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
}

void test_physical_output_runtime() {
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
