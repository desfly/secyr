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

std::uint32_t bench_delay_seen{};
FakeBackend* bench_transition_backend{};
bool bench_transition_cold_open{};
void fake_bench_delay(std::uint32_t duration_ms) {
    bench_delay_seen = duration_ms;
    if (bench_transition_backend != nullptr && bench_transition_cold_open) {
        bench_transition_backend->set_limit(hg::PhysicalInputChannel::ColdValveOpenLimit, true);
    }
}

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

    // Fresh/unverified hardware is still a successfully initialized OFF-only
    // runtime. This keeps Factory Reset/service safe, but commissioning cannot
    // energize anything before signed hardware verification.
    FakeBackend unverified_backend;
    hg::PhysicalOutputRuntime unverified_runtime;
    hg::HardwareVerificationRecord missing_hardware{};
    TEST_CHECK(unverified_runtime.initialize(
        unverified_backend, missing_hardware, commissioning, blocked));
    TEST_CHECK(unverified_runtime.state().status == hg::PhysicalOutputStatus::InvalidHardware);
    for (int channel = 0; channel < 8; ++channel) {
        TEST_CHECK(unverified_backend.levels.count(channel) == 1U);
        TEST_CHECK(!unverified_backend.levels[channel]);
    }
    TEST_CHECK(unverified_runtime.set_maintenance_mode(true));
    bench_delay_seen = 0;
    TEST_CHECK(!unverified_runtime.bench_pulse(
        hg::PhysicalOutputChannel::Siren, 100, &fake_bench_delay));
    TEST_CHECK(bench_delay_seen == 0U);
    TEST_CHECK(unverified_runtime.lockout_fail_closed());
    TEST_CHECK(unverified_runtime.state().safety_fault_latched);

    // Hardware verification alone is still not permission to energize a bench
    // output. A successful dry-run is mandatory for every bench channel.
    auto before_dry_run = commissioning;
    before_dry_run.successful_dry_runs = 0;
    before_dry_run.successful_actuator_tests = 0;
    FakeBackend bench_backend;
    hg::PhysicalOutputRuntime bench_runtime;
    TEST_CHECK(bench_runtime.initialize(bench_backend, hardware, before_dry_run, blocked));
    TEST_CHECK(bench_runtime.set_maintenance_mode(true));
    TEST_CHECK(!bench_runtime.bench_pulse(
        hg::PhysicalOutputChannel::Siren, 100, &fake_bench_delay));

    // After dry-run, non-valve bench channels are allowed, but valve coils stay
    // blocked until the measured limit polarity + both travel timeouts exist.
    auto dry_run_only = commissioning;
    dry_run_only.successful_actuator_tests = 0;
    dry_run_only.valve_limit_polarity_verified = false;
    dry_run_only.cold_valve_travel_timeout_ms = 0;
    dry_run_only.hot_valve_travel_timeout_ms = 0;
    TEST_CHECK(bench_runtime.update_control_state(hardware, dry_run_only, blocked));
    bench_delay_seen = 0;
    TEST_CHECK(bench_runtime.bench_pulse(
        hg::PhysicalOutputChannel::Siren, 100, &fake_bench_delay));
    TEST_CHECK(bench_delay_seen == 100U);
    TEST_CHECK(!bench_runtime.bench_pulse(
        hg::PhysicalOutputChannel::ColdValveOpen, 100, &fake_bench_delay));

    // Valve profile permits a bounded physical pulse, but a direction only
    // counts when the target GPB end switch transitions from inactive before
    // the pulse to active afterwards. A pulse alone is never actuator proof.
    auto profiled = commissioning;
    profiled.successful_actuator_tests = 0;
    TEST_CHECK(bench_runtime.update_control_state(hardware, profiled, blocked));
    bench_delay_seen = 0;
    TEST_CHECK(!bench_runtime.bench_pulse(
        hg::PhysicalOutputChannel::ColdValveOpen, 100, &fake_bench_delay));
    TEST_CHECK(bench_delay_seen == 100U);
    for (int channel = 0; channel < 8; ++channel) TEST_CHECK(!bench_backend.levels[channel]);

    bench_transition_backend = &bench_backend;
    bench_transition_cold_open = true;
    bench_delay_seen = 0;
    TEST_CHECK(bench_runtime.bench_pulse(
        hg::PhysicalOutputChannel::ColdValveOpen, 100, &fake_bench_delay));
    TEST_CHECK(bench_delay_seen == 100U);
    bench_transition_cold_open = false;
    bench_transition_backend = nullptr;

    // Once the target end stop is already active, another pulse toward that
    // stop is rejected before energizing the coil and cannot become evidence.
    bench_delay_seen = 0;
    TEST_CHECK(!bench_runtime.bench_pulse(
        hg::PhysicalOutputChannel::ColdValveOpen, 100, &fake_bench_delay));
    TEST_CHECK(bench_delay_seen == 0U);

    TEST_CHECK(!bench_runtime.bench_pulse(
        hg::PhysicalOutputChannel::Siren,
        hg::PhysicalOutputRuntime::kMaxBenchPulseMs + 1U,
        &fake_bench_delay));
    for (int channel = 0; channel < 8; ++channel) {
        TEST_CHECK(!bench_backend.levels[channel]);
    }
    TEST_CHECK(!bench_runtime.state().outputs_enabled);

    // Contradictory end switches during commissioning are a latched physical
    // safety fault, exactly as in normal runtime supervision.
    bench_backend.set_limit(hg::PhysicalInputChannel::ColdValveClosedLimit, true);
    TEST_CHECK(!bench_runtime.bench_pulse(
        hg::PhysicalOutputChannel::ColdValveOpen, 100, &fake_bench_delay));
    TEST_CHECK(bench_runtime.state().status == hg::PhysicalOutputStatus::ValveSafetyFault);
    TEST_CHECK(bench_runtime.state().safety_fault_latched);
    for (int channel = 0; channel < 8; ++channel) TEST_CHECK(!bench_backend.levels[channel]);

    FakeBackend backend;
    hg::PhysicalOutputRuntime runtime;
    TEST_CHECK(runtime.initialize(backend, hardware, commissioning, blocked));
    TEST_CHECK(!runtime.state().outputs_enabled);
    for (int channel = 0; channel < 8; ++channel) TEST_CHECK(!backend.levels[channel]);

    // Dynamic commissioning/readiness update enables normal supervision without
    // reboot; all physical outputs remain OFF until a fresh command revision.
    TEST_CHECK(runtime.update_control_state(hardware, commissioning, ready));
    TEST_CHECK(runtime.state().status == hg::PhysicalOutputStatus::Ready);
    TEST_CHECK(runtime.synchronize(model, 100));
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveOpen)]);
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveClose)]);
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::HotValveOpen)]);
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::HotValveClose)]);

    // Maintenance forces normal outputs OFF and blocks the supervisor. Leaving
    // it does not re-run already consumed command revisions.
    TEST_CHECK(model.set_output_active(1, true, 101));
    TEST_CHECK(runtime.synchronize(model, 101));
    TEST_CHECK(backend.levels[ch(hg::PhysicalOutputChannel::Siren)]);
    TEST_CHECK(runtime.set_maintenance_mode(true));
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::Siren)]);
    TEST_CHECK(runtime.synchronize(model, 102));
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::Siren)]);
    TEST_CHECK(runtime.set_maintenance_mode(false));
    TEST_CHECK(runtime.synchronize(model, 103));
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::Siren)]);

    // Explicit OPEN starts motion.
    TEST_CHECK(model.set_output_active(2, true, 110));
    TEST_CHECK(runtime.synchronize(model, 110));
    TEST_CHECK(backend.levels[ch(hg::PhysicalOutputChannel::ColdValveOpen)]);
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveClose)]);
    TEST_CHECK(runtime.state().cold_valve.direction == hg::ValveMotionDirection::Opening);

    backend.set_limit(hg::PhysicalInputChannel::ColdValveOpenLimit, true);
    TEST_CHECK(runtime.synchronize(model, 150));
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveOpen)]);
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveClose)]);
    TEST_CHECK(runtime.state().cold_valve.direction == hg::ValveMotionDirection::Stopped);
    const auto limit_stops = runtime.state().limit_stops;

    TEST_CHECK(runtime.synchronize(model, 200));
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveOpen)]);
    TEST_CHECK(runtime.state().limit_stops == limit_stops);

    TEST_CHECK(model.set_output_active(2, true, 210));
    TEST_CHECK(runtime.synchronize(model, 210));
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveOpen)]);

    backend.set_limit(hg::PhysicalInputChannel::ColdValveOpenLimit, false);
    TEST_CHECK(model.set_output_active(2, false, 220));
    TEST_CHECK(runtime.synchronize(model, 220));
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveOpen)]);
    TEST_CHECK(backend.levels[ch(hg::PhysicalOutputChannel::ColdValveClose)]);
    TEST_CHECK(runtime.state().cold_valve.direction == hg::ValveMotionDirection::Closing);

    backend.set_limit(hg::PhysicalInputChannel::ColdValveClosedLimit, true);
    TEST_CHECK(runtime.synchronize(model, 250));
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveOpen)]);
    TEST_CHECK(!backend.levels[ch(hg::PhysicalOutputChannel::ColdValveClose)]);

    backend.set_limit(hg::PhysicalInputChannel::ColdValveClosedLimit, false);
    TEST_CHECK(model.set_output_active(3, true, 1000));
    TEST_CHECK(runtime.synchronize(model, 1000));
    TEST_CHECK(backend.levels[ch(hg::PhysicalOutputChannel::HotValveOpen)]);
    TEST_CHECK(!runtime.synchronize(model, 2200));
    const auto timeout_state = runtime.state();
    TEST_CHECK(timeout_state.status == hg::PhysicalOutputStatus::ValveTimeout);
    TEST_CHECK(timeout_state.safety_fault_latched);
    TEST_CHECK(timeout_state.valve_timeouts == 1U);
    for (int channel = 0; channel < 8; ++channel) TEST_CHECK(!backend.levels[channel]);

    FakeBackend conflicting_backend;
    conflicting_backend.set_limit(hg::PhysicalInputChannel::ColdValveOpenLimit, true);
    conflicting_backend.set_limit(hg::PhysicalInputChannel::ColdValveClosedLimit, true);
    hg::PhysicalOutputRuntime conflicting;
    TEST_CHECK(conflicting.initialize(conflicting_backend, hardware, commissioning, ready));
    TEST_CHECK(!conflicting.synchronize(model, 3000));
    TEST_CHECK(conflicting.state().status == hg::PhysicalOutputStatus::ValveSafetyFault);
    TEST_CHECK(conflicting.state().safety_fault_latched);

    FakeBackend read_failure_backend;
    hg::PhysicalOutputRuntime read_failure;
    TEST_CHECK(read_failure.initialize(read_failure_backend, hardware, commissioning, ready));
    read_failure_backend.fail_reads = true;
    TEST_CHECK(!read_failure.synchronize(model, 4000));
    TEST_CHECK(read_failure.state().status == hg::PhysicalOutputStatus::BackendError);
    TEST_CHECK(read_failure.state().safety_fault_latched);

    FakeBackend blocked_backend;
    hg::PhysicalOutputRuntime blocked_runtime;
    TEST_CHECK(blocked_runtime.initialize(blocked_backend, hardware, commissioning, ready));
    TEST_CHECK(blocked_runtime.update_control_state(hardware, commissioning, blocked));
    TEST_CHECK(blocked_runtime.synchronize(model, 5000));
    for (int channel = 0; channel < 8; ++channel) TEST_CHECK(!blocked_backend.levels[channel]);
    TEST_CHECK(!blocked_runtime.state().outputs_enabled);

    FakeBackend failing;
    failing.fail_writes = true;
    hg::PhysicalOutputRuntime broken;
    TEST_CHECK(!broken.initialize(failing, hardware, commissioning, ready));
    TEST_CHECK(broken.state().status == hg::PhysicalOutputStatus::BackendError);
    TEST_CHECK(broken.state().safety_fault_latched);
}
