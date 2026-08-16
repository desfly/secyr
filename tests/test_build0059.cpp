#include "test_framework.hpp"
#include "homeguard/physical_output_runtime.hpp"

#include <cstdint>
#include <map>

namespace {
class DiagnosticBackend final : public hg::PhysicalOutputBackend {
public:
    bool configure_output(int channel, bool initial_level) override {
        levels[channel] = initial_level;
        return true;
    }
    bool write_output(int channel, bool level) override {
        levels[channel] = level;
        return true;
    }
    bool read_inputs(std::uint8_t* value) override {
        if (fail_reads || value == nullptr) return false;
        *value = inputs;
        return true;
    }
    void set_limit(hg::PhysicalInputChannel channel, bool active) {
        const auto mask = static_cast<std::uint8_t>(1U << static_cast<unsigned>(channel));
        if (active) inputs &= static_cast<std::uint8_t>(~mask);  // active-low
        else inputs |= mask;
    }

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
}

void test_build0059() {
    hg::SystemEventBus bus;
    hg::SystemModel model(bus);
    TEST_CHECK(model.add_output(1, hg::ModelOutputType::Siren));
    TEST_CHECK(model.add_output(2, hg::ModelOutputType::Valve));
    TEST_CHECK(model.add_output(3, hg::ModelOutputType::Valve));

    auto hardware = verified_hardware();
    auto commissioning = verified_commissioning();
    hg::BootReadinessReport ready{};
    ready.status = hg::BootReadinessStatus::ReadyForPhysicalOutputs;

    DiagnosticBackend backend;
    hg::PhysicalOutputRuntime runtime;
    TEST_CHECK(runtime.initialize(backend, hardware, commissioning, ready));
    TEST_CHECK(runtime.synchronize(model, 10));

    auto state = runtime.state();
    TEST_CHECK(state.limit_inputs_valid);
    TEST_CHECK(state.raw_limit_inputs == 0xFFU);
    TEST_CHECK(!state.cold_open_limit);
    TEST_CHECK(!state.cold_closed_limit);
    TEST_CHECK(!state.hot_open_limit);
    TEST_CHECK(!state.hot_closed_limit);

    backend.set_limit(hg::PhysicalInputChannel::ColdValveOpenLimit, true);
    TEST_CHECK(runtime.synchronize(model, 20));
    state = runtime.state();
    TEST_CHECK(state.limit_inputs_valid);
    TEST_CHECK(state.raw_limit_inputs == 0xFEU);
    TEST_CHECK(state.cold_open_limit);
    TEST_CHECK(!state.cold_closed_limit);

    backend.set_limit(hg::PhysicalInputChannel::ColdValveClosedLimit, true);
    TEST_CHECK(!runtime.synchronize(model, 30));
    state = runtime.state();
    TEST_CHECK(state.limit_inputs_valid);
    TEST_CHECK(state.raw_limit_inputs == 0xFCU);
    TEST_CHECK(state.cold_open_limit);
    TEST_CHECK(state.cold_closed_limit);
    TEST_CHECK(state.status == hg::PhysicalOutputStatus::ValveSafetyFault);
    TEST_CHECK(state.safety_fault_latched);

    DiagnosticBackend read_failure_backend;
    hg::PhysicalOutputRuntime read_failure;
    TEST_CHECK(read_failure.initialize(read_failure_backend, hardware, commissioning, ready));
    read_failure_backend.fail_reads = true;
    TEST_CHECK(!read_failure.synchronize(model, 40));
    const auto read_failure_state = read_failure.state();
    TEST_CHECK(!read_failure_state.limit_inputs_valid);
    TEST_CHECK(read_failure_state.status == hg::PhysicalOutputStatus::BackendError);
    TEST_CHECK(read_failure_state.safety_fault_latched);
}
