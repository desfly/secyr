#include "test_framework.hpp"
#include "homeguard/physical_output_runtime.hpp"

#include <cstdint>
#include <map>

namespace {
class FakeBackend final : public hg::PhysicalOutputBackend {
public:
    bool configure_output(int channel, bool initial_level) override {
        levels[channel] = initial_level;
        return !fail_writes;
    }
    bool write_output(int channel, bool level) override {
        if (fail_writes) return false;
        levels[channel] = level;
        return true;
    }
    bool read_inputs(std::uint8_t* value) override {
        if (fail_reads || value == nullptr) return false;
        *value = inputs;
        return true;
    }

    void set_limit(hg::PhysicalInputChannel channel, bool active, bool active_low = true) {
        const auto mask = static_cast<std::uint8_t>(1U << static_cast<unsigned>(channel));
        const bool high = active_low ? !active : active;
        if (high) inputs |= mask;
        else inputs &= static_cast<std::uint8_t>(~mask);
    }

    bool fail_writes{};
    bool fail_reads{};
    std::uint8_t inputs{0xFFU};
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

hg::CommissioningPersistentState verified_commissioning() {
    hg::CommissioningPersistentState state{};
    state.gpio_map_verified = true;
    state.active_polarity_verified = true;
    state.successful_dry_runs = 1;
    state.successful_actuator_tests = 1;
    state.last_verified_at_ms = 1;
    state.valve_limit_polarity_verified = true;
    state.valve_limits_active_low = true;
    state.cold_valve_travel_timeout_ms = 1000;
    state.hot_valve_travel_timeout_ms = 1200;
    return state;
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
    auto commissioning = verified_commissioning();
    hg::BootReadinessReport blocked{};
    hg::BootReadinessReport ready{};
    ready.status = hg::BootReadinessStatus::ReadyForPhysicalOutputs;

    FakeBackend backend;
    hg::PhysicalOutputRuntime runtime;
    TEST_CHECK(runtime.initialize(backend, hardware, commissioning, blocked));
    TEST_CHECK(!runtime.state().outputs_enabled);

    for (int channel = 0; channel < 8; ++channel) {
        TEST_CHECK(!backend.levels[channel]);
    }

    // Becoming ready without a valve command is STOP, not implicit CLOSE.
    TEST_CHECK(runtime.synchronize(model, ready, 100));
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveOpen)]);
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveClose)]);
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::HotValveOpen)]);
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::HotValveClose)]);

    // Explicit OPEN starts motion.
    TEST_CHECK(model.set_output_active(2, true, 110));
    TEST_CHECK(runtime.synchronize(model, ready, 110));
    TEST_CHECK(backend.levels[ch(hg::PhysicalOutputChannel::ColdValveOpen)]);
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveClose)]);
    TEST_CHECK(runtime.state().cold_valve.direction == hg::ValveMotionDirection::Opening);

    // Target end-switch immediately stops both directions.
    backend.set_limit(hg::PhysicalInputChannel::ColdValveOpenLimit, true);
    TEST_CHECK(runtime.synchronize(model, ready, 150));
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveOpen)]);
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveClose)]);
    TEST_CHECK(runtime.state().cold_valve.direction == hg::ValveMotionDirection::Stopped);
    const auto limit_stops = runtime.state().limit_stops;

    // Same command revision cannot re-start after the end-switch stop.
    TEST_CHECK(runtime.synchronize(model, ready, 200));
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveOpen)]);
    TEST_CHECK(runtime.state().limit_stops == limit_stops);

    // Repeated explicit OPEN is a new revision but the active target switch
    // consumes it without energizing the motor.
    TEST_CHECK(model.set_output_active(2, true, 210));
    TEST_CHECK(runtime.synchronize(model, ready, 210));
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveOpen)]);

    // Clear OPEN limit and issue CLOSE: break-before-make selects CLOSE only.
    backend.set_limit(hg::PhysicalInputChannel::ColdValveOpenLimit, false);
    TEST_CHECK(model.set_output_active(2, false, 220));
    TEST_CHECK(runtime.synchronize(model, ready, 220));
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveOpen)]);
    TEST_CHECK(backend.levels[ch(hg::PhysicalOutputChannel::ColdValveClose)]);
    TEST_CHECK(runtime.state().cold_valve.direction == hg::ValveMotionDirection::Closing);

    backend.set_limit(hg::PhysicalInputChannel::ColdValveClosedLimit, true);
    TEST_CHECK(runtime.synchronize(model, ready, 250));
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveOpen)]);
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveClose)]);

    // Timeout is a latched safety fault and forces all GPA outputs OFF.
    backend.set_limit(hg::PhysicalInputChannel::ColdValveClosedLimit, false);
    TEST_CHECK(model.set_output_active(3, true, 1000));
    TEST_CHECK(runtime.synchronize(model, ready, 1000));
    TEST_CHECK(backend.levels[ch(hg::PhysicalOutputChannel::HotValveOpen)]);
    TEST_CHECK(!runtime.synchronize(model, ready, 2200));
    const auto timeout_state = runtime.state();
    TEST_CHECK(timeout_state.status == hg::PhysicalOutputStatus::ValveTimeout);
    TEST_CHECK(timeout_state.safety_fault_latched);
    TEST_CHECK(timeout_state.valve_timeouts == 1U);
    for (int channel = 0; channel < 8; ++channel) {
        TEST_CHECK(!backend.levels[channel]);
    }

    // Contradictory OPEN+CLOSED end-switches latch valve_safety_fault.
    FakeBackend conflicting_backend;
    conflicting_backend.set_limit(hg::PhysicalInputChannel::ColdValveOpenLimit, true);
    conflicting_backend.set_limit(hg::PhysicalInputChannel::ColdValveClosedLimit, true);
    hg::PhysicalOutputRuntime conflicting;
    TEST_CHECK(conflicting.initialize(conflicting_backend, hardware, commissioning, ready));
    TEST_CHECK(!conflicting.synchronize(model, ready, 3000));
    TEST_CHECK(conflicting.state().status == hg::PhysicalOutputStatus::ValveSafetyFault);
    TEST_CHECK(conflicting.state().safety_fault_latched);

    // Loss of the supervised input path is fail-closed too.
    FakeBackend read_failure_backend;
    hg::PhysicalOutputRuntime read_failure;
    TEST_CHECK(read_failure.initialize(read_failure_backend, hardware, commissioning, ready));
    read_failure_backend.fail_reads = true;
    TEST_CHECK(!read_failure.synchronize(model, ready, 4000));
    TEST_CHECK(read_failure.state().status == hg::PhysicalOutputStatus::BackendError);
    TEST_CHECK(read_failure.state().safety_fault_latched);

    // Readiness loss forces STOP/OFF even without an actuator fault.
    FakeBackend blocked_backend;
    hg::PhysicalOutputRuntime blocked_runtime;
    TEST_CHECK(blocked_runtime.initialize(blocked_backend, hardware, commissioning, ready));
    TEST_CHECK(blocked_runtime.synchronize(model, blocked, 5000));
    for (int channel = 0; channel < 8; ++channel) {
        TEST_CHECK(!blocked_backend.levels[channel]);
    }
    TEST_CHECK(!blocked_runtime.state().outputs_enabled);

    FakeBackend failing;
    failing.fail_writes = true;
    hg::PhysicalOutputRuntime broken;
    TEST_CHECK(!broken.initialize(failing, hardware, commissioning, ready));
    TEST_CHECK(broken.state().status == hg::PhysicalOutputStatus::BackendError);
    TEST_CHECK(broken.state().safety_fault_latched);
}
