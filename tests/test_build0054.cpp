#include "test_framework.hpp"
#include "homeguard/physical_output_runtime.hpp"

#include <map>

namespace {
class FakeBackend final : public hg::PhysicalOutputBackend {
public:
    bool configure_output(int channel, bool initial_level) override {
        levels[channel] = initial_level;
        return !fail;
    }
    bool write_output(int channel, bool level) override {
        if (fail) return false;
        levels[channel] = level;
        return true;
    }
    bool fail{};
    std::map<int, bool> levels;
};

hg::HardwareVerificationRecord verified_hardware() {
    hg::HardwareVerificationRecord record{};
    record.pins.i2c_sda = 4;
    record.pins.i2c_scl = 5;
    record.pins.w5500_mosi = 11;
    record.pins.w5500_miso = 13;
    record.pins.w5500_sclk = 12;
    record.pins.w5500_cs = 10;
    record.pins.w5500_int = 9;
    record.pins.w5500_rst = 8;
    record.pins.service_button = 21;
    record.active_polarity_verified = true;
    record.verified_at_ms = 1;
    record.profile_crc32 = hg::hardware_profile_crc32(record);
    return record;
}

int ch(hg::PhysicalOutputChannel channel) {
    return static_cast<int>(channel);
}
}

void test_build0054() {
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

    // All eight MCP Port-A logical channels are configured OFF.
    for (int channel = 0; channel < 8; ++channel) {
        TEST_CHECK(!backend.levels[channel]);
    }

    hg::BootReadinessReport ready{};
    ready.status = hg::BootReadinessStatus::ReadyForPhysicalOutputs;

    // Merely becoming ready must not move either valve. No explicit valve
    // command has been issued yet, so both OPEN/CLOSE pairs remain STOP/OFF.
    TEST_CHECK(runtime.synchronize(model, ready));
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveOpen)]);
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveClose)]);
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::HotValveOpen)]);
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::HotValveClose)]);

    TEST_CHECK(model.set_output_active(1, true, 10));
    TEST_CHECK(model.set_output_active(2, true, 10));
    TEST_CHECK(model.set_output_active(3, true, 10));
    TEST_CHECK(runtime.synchronize(model, ready));
    TEST_CHECK(runtime.state().outputs_enabled);
    TEST_CHECK(backend.levels[ch(hg::PhysicalOutputChannel::Siren)]);
    TEST_CHECK(backend.levels[ch(hg::PhysicalOutputChannel::ColdValveOpen)]);
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveClose)]);
    TEST_CHECK(backend.levels[ch(hg::PhysicalOutputChannel::HotValveOpen)]);
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::HotValveClose)]);

    // false is a real CLOSE command once commanded=true, not merely an inactive
    // relay. Break-before-make clears OPEN before asserting CLOSE.
    TEST_CHECK(model.set_output_active(3, false, 11));
    TEST_CHECK(runtime.synchronize(model, ready));
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::HotValveOpen)]);
    TEST_CHECK(backend.levels[ch(hg::PhysicalOutputChannel::HotValveClose)]);

    TEST_CHECK(runtime.synchronize(model, blocked));
    for (int channel = 0; channel < 8; ++channel) {
        TEST_CHECK(!backend.levels[channel]);
    }
    TEST_CHECK(!runtime.state().outputs_enabled);

    FakeBackend failing;
    failing.fail = true;
    hg::PhysicalOutputRuntime broken;
    TEST_CHECK(!broken.initialize(failing, hardware, ready));
    TEST_CHECK(broken.state().status == hg::PhysicalOutputStatus::BackendError);
}
