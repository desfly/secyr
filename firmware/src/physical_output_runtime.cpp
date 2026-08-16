#include "homeguard/physical_output_runtime.hpp"

#include <array>

namespace hg {
namespace {

constexpr int channel_number(PhysicalOutputChannel channel) noexcept
{
    return static_cast<int>(channel);
}

constexpr int input_number(PhysicalInputChannel channel) noexcept
{
    return static_cast<int>(channel);
}

constexpr std::array<PhysicalOutputChannel, 8> kAllChannels{
    PhysicalOutputChannel::CorridorLight,
    PhysicalOutputChannel::Siren,
    PhysicalOutputChannel::ColdValveOpen,
    PhysicalOutputChannel::ColdValveClose,
    PhysicalOutputChannel::HotValveOpen,
    PhysicalOutputChannel::HotValveClose,
    PhysicalOutputChannel::Reserve1,
    PhysicalOutputChannel::Reserve2,
};

bool bit_active(std::uint8_t value, int bit, bool active_low) noexcept
{
    const bool high = (value & static_cast<std::uint8_t>(1U << static_cast<unsigned>(bit))) != 0U;
    return active_low ? !high : high;
}

bool valve_bench_channel(PhysicalOutputChannel channel) noexcept
{
    switch (channel) {
        case PhysicalOutputChannel::ColdValveOpen:
        case PhysicalOutputChannel::ColdValveClose:
        case PhysicalOutputChannel::HotValveOpen:
        case PhysicalOutputChannel::HotValveClose:
            return true;
        case PhysicalOutputChannel::CorridorLight:
        case PhysicalOutputChannel::Siren:
        case PhysicalOutputChannel::Reserve1:
        case PhysicalOutputChannel::Reserve2:
            return false;
    }
    return false;
}

}  // namespace

bool PhysicalOutputRuntime::initialize(
    PhysicalOutputBackend& backend,
    const HardwareVerificationRecord& hardware,
    const CommissioningPersistentState& commissioning,
    const BootReadinessReport& readiness)
{
    std::scoped_lock lock(mutex_);
    backend_ = &backend;
    hardware_ = hardware;
    commissioning_ = commissioning;
    readiness_ = readiness;
    hardware_verified_ = false;
    state_ = {};
    siren_command_revision_ = 0U;

    for (const auto channel : kAllChannels) {
        if (!backend_->configure_output(channel_number(channel), false)) {
            ++state_.failures;
            state_.status = PhysicalOutputStatus::BackendError;
            state_.safety_fault_latched = true;
            return false;
        }
    }

    hardware_verified_ = hardware_verification_allows_outputs(hardware_);
    if (!hardware_verified_) {
        state_.status = PhysicalOutputStatus::InvalidHardware;
        state_.outputs_enabled = false;
        return true;
    }

    if (!readiness_.outputs_allowed() ||
        !commissioning_state_allows_physical_outputs(commissioning_)) {
        state_.status = PhysicalOutputStatus::FailClosed;
        state_.outputs_enabled = false;
        return true;
    }

    state_.outputs_enabled = true;
    state_.status = PhysicalOutputStatus::Ready;
    return true;
}

bool PhysicalOutputRuntime::update_control_state(
    const HardwareVerificationRecord& hardware,
    const CommissioningPersistentState& commissioning,
    const BootReadinessReport& readiness)
{
    std::scoped_lock lock(mutex_);
    if (backend_ == nullptr) return false;

    hardware_ = hardware;
    commissioning_ = commissioning;
    readiness_ = readiness;
    hardware_verified_ = hardware_verification_allows_outputs(hardware_);

    if (state_.safety_fault_latched) return false;

    if (!hardware_verified_) {
        if (!force_safe_locked()) return false;
        state_.status = PhysicalOutputStatus::InvalidHardware;
        state_.outputs_enabled = false;
        return true;
    }

    if (state_.maintenance_mode || !readiness_.outputs_allowed() ||
        !commissioning_state_allows_physical_outputs(commissioning_)) {
        if (!force_safe_locked()) return false;
        state_.status = PhysicalOutputStatus::FailClosed;
        state_.outputs_enabled = false;
        return true;
    }

    if (!force_safe_locked()) return false;
    state_.status = PhysicalOutputStatus::Ready;
    state_.outputs_enabled = true;
    return true;
}

bool PhysicalOutputRuntime::set_maintenance_mode(bool active)
{
    std::scoped_lock lock(mutex_);
    if (backend_ == nullptr || state_.safety_fault_latched) return false;

    if (active) {
        if (!force_safe_locked()) return false;
        state_.maintenance_mode = true;
        state_.status = hardware_verified_
            ? PhysicalOutputStatus::FailClosed
            : PhysicalOutputStatus::InvalidHardware;
        state_.outputs_enabled = false;
        return true;
    }

    state_.maintenance_mode = false;
    if (!force_safe_locked()) return false;
    if (!hardware_verified_) {
        state_.status = PhysicalOutputStatus::InvalidHardware;
        state_.outputs_enabled = false;
    } else if (readiness_.outputs_allowed() &&
               commissioning_state_allows_physical_outputs(commissioning_)) {
        state_.status = PhysicalOutputStatus::Ready;
        state_.outputs_enabled = true;
    } else {
        state_.status = PhysicalOutputStatus::FailClosed;
        state_.outputs_enabled = false;
    }
    return true;
}

bool PhysicalOutputRuntime::write_safe_locked(PhysicalOutputChannel channel)
{
    if (backend_ == nullptr || !backend_->write_output(channel_number(channel), false)) {
        ++state_.failures;
        return false;
    }
    ++state_.writes;
    return true;
}

bool PhysicalOutputRuntime::write_logical_locked(PhysicalOutputChannel channel, bool active)
{
    if (backend_ == nullptr || !backend_->write_output(channel_number(channel), active)) {
        ++state_.failures;
        return false;
    }
    ++state_.writes;
    return true;
}

bool PhysicalOutputRuntime::force_safe_locked()
{
    if (backend_ == nullptr) return false;

    bool ok = true;
    for (const auto channel : kAllChannels) {
        ok = write_safe_locked(channel) && ok;
    }

    state_.cold_valve.direction = ValveMotionDirection::Stopped;
    state_.cold_valve.started_at_ms = 0;
    state_.cold_valve.timeout_ms = 0;
    state_.hot_valve.direction = ValveMotionDirection::Stopped;
    state_.hot_valve.started_at_ms = 0;
    state_.hot_valve.timeout_ms = 0;
    state_.outputs_enabled = false;

    if (!ok) {
        state_.status = PhysicalOutputStatus::BackendError;
        state_.safety_fault_latched = true;
    }
    return ok;
}

bool PhysicalOutputRuntime::latch_fault_locked(PhysicalOutputStatus status)
{
    const bool safe = force_safe_locked();
    state_.safety_fault_latched = true;
    state_.status = safe ? status : PhysicalOutputStatus::BackendError;
    return false;
}

bool PhysicalOutputRuntime::read_limits_locked(LimitSnapshot& limits)
{
    if (backend_ == nullptr || !commissioning_.valve_limit_polarity_verified) {
        return false;
    }

    std::uint8_t raw = 0;
    if (!backend_->read_inputs(&raw)) {
        ++state_.failures;
        return false;
    }

    const bool active_low = commissioning_.valve_limits_active_low;
    limits.cold_open = bit_active(raw, input_number(PhysicalInputChannel::ColdValveOpenLimit), active_low);
    limits.cold_closed = bit_active(raw, input_number(PhysicalInputChannel::ColdValveClosedLimit), active_low);
    limits.hot_open = bit_active(raw, input_number(PhysicalInputChannel::HotValveOpenLimit), active_low);
    limits.hot_closed = bit_active(raw, input_number(PhysicalInputChannel::HotValveClosedLimit), active_low);
    return true;
}

bool PhysicalOutputRuntime::stop_valve_locked(
    ValveMotionState& motion,
    PhysicalOutputChannel open_channel,
    PhysicalOutputChannel close_channel,
    bool reached_limit)
{
    bool ok = write_logical_locked(open_channel, false);
    ok = write_logical_locked(close_channel, false) && ok;
    motion.direction = ValveMotionDirection::Stopped;
    motion.started_at_ms = 0;
    motion.timeout_ms = 0;
    if (reached_limit) ++state_.limit_stops;
    return ok;
}

bool PhysicalOutputRuntime::process_valve_locked(
    ValveMotionState& motion,
    PhysicalOutputChannel open_channel,
    PhysicalOutputChannel close_channel,
    bool open_limit,
    bool close_limit,
    const OutputRecord* output,
    std::uint32_t configured_timeout_ms,
    std::uint64_t now_ms)
{
    if (motion.direction != ValveMotionDirection::Stopped) {
        const bool target_reached =
            (motion.direction == ValveMotionDirection::Opening && open_limit) ||
            (motion.direction == ValveMotionDirection::Closing && close_limit);
        if (target_reached) {
            if (!stop_valve_locked(motion, open_channel, close_channel, true)) {
                return latch_fault_locked(PhysicalOutputStatus::BackendError);
            }
        } else if (motion.timeout_ms == 0U ||
                   now_ms - motion.started_at_ms >= motion.timeout_ms) {
            ++state_.valve_timeouts;
            return latch_fault_locked(PhysicalOutputStatus::ValveTimeout);
        }
    }

    if (output == nullptr || !output->commanded ||
        output->command_revision == motion.command_revision) {
        return true;
    }

    if (motion.direction != ValveMotionDirection::Stopped &&
        !stop_valve_locked(motion, open_channel, close_channel, false)) {
        return latch_fault_locked(PhysicalOutputStatus::BackendError);
    }

    motion.command_revision = output->command_revision;
    if (configured_timeout_ms == 0U) {
        return latch_fault_locked(PhysicalOutputStatus::ValveSafetyFault);
    }

    const bool target_limit = output->active ? open_limit : close_limit;
    if (target_limit) {
        ++state_.limit_stops;
        return true;
    }

    if (output->active) {
        if (!write_logical_locked(close_channel, false) ||
            !write_logical_locked(open_channel, true)) {
            return latch_fault_locked(PhysicalOutputStatus::BackendError);
        }
        motion.direction = ValveMotionDirection::Opening;
    } else {
        if (!write_logical_locked(open_channel, false) ||
            !write_logical_locked(close_channel, true)) {
            return latch_fault_locked(PhysicalOutputStatus::BackendError);
        }
        motion.direction = ValveMotionDirection::Closing;
    }

    motion.started_at_ms = now_ms;
    motion.timeout_ms = configured_timeout_ms;
    return true;
}

bool PhysicalOutputRuntime::bench_channel_allowed(PhysicalOutputChannel channel) noexcept
{
    switch (channel) {
        case PhysicalOutputChannel::CorridorLight:
        case PhysicalOutputChannel::Siren:
        case PhysicalOutputChannel::ColdValveOpen:
        case PhysicalOutputChannel::ColdValveClose:
        case PhysicalOutputChannel::HotValveOpen:
        case PhysicalOutputChannel::HotValveClose:
            return true;
        case PhysicalOutputChannel::Reserve1:
        case PhysicalOutputChannel::Reserve2:
            return false;
    }
    return false;
}

bool PhysicalOutputRuntime::bench_pulse(
    PhysicalOutputChannel channel,
    std::uint32_t duration_ms,
    BenchDelayFn delay_fn)
{
    std::scoped_lock lock(mutex_);
    if (backend_ == nullptr || state_.safety_fault_latched || !state_.maintenance_mode ||
        delay_fn == nullptr || duration_ms == 0U || duration_ms > kMaxBenchPulseMs ||
        !bench_channel_allowed(channel)) {
        return false;
    }

    // A maintenance endpoint is not itself proof that energizing hardware is
    // safe. Every bench pulse therefore requires a signed HW-678 verification
    // plus a completed dry-run. Valve directions additionally require the
    // measured limit polarity and non-zero travel timeouts before any coil can
    // be energized. Normal outputs remain fail-closed throughout commissioning.
    const bool dry_run_verified =
        hardware_verified_ &&
        commissioning_.gpio_map_verified &&
        commissioning_.active_polarity_verified &&
        commissioning_.successful_dry_runs > 0U;
    if (!dry_run_verified) return false;

    if (valve_bench_channel(channel) &&
        (!commissioning_.valve_limit_polarity_verified ||
         commissioning_.cold_valve_travel_timeout_ms == 0U ||
         commissioning_.hot_valve_travel_timeout_ms == 0U)) {
        return false;
    }

    if (!force_safe_locked()) return false;
    if (!write_logical_locked(channel, true)) {
        return latch_fault_locked(PhysicalOutputStatus::BackendError);
    }

    delay_fn(duration_ms);

    if (!force_safe_locked()) return false;
    state_.maintenance_mode = true;
    state_.status = hardware_verified_
        ? PhysicalOutputStatus::FailClosed
        : PhysicalOutputStatus::InvalidHardware;
    state_.outputs_enabled = false;
    return true;
}

bool PhysicalOutputRuntime::force_safe()
{
    std::scoped_lock lock(mutex_);
    const bool ok = force_safe_locked();
    if (ok && !state_.safety_fault_latched) {
        state_.status = state_.maintenance_mode
            ? (hardware_verified_ ? PhysicalOutputStatus::FailClosed : PhysicalOutputStatus::InvalidHardware)
            : PhysicalOutputStatus::FailClosed;
    }
    return ok;
}

bool PhysicalOutputRuntime::lockout_fail_closed()
{
    std::scoped_lock lock(mutex_);
    const bool ok = force_safe_locked();
    state_.safety_fault_latched = true;
    state_.status = ok ? PhysicalOutputStatus::FailClosed : PhysicalOutputStatus::BackendError;
    return ok;
}

bool PhysicalOutputRuntime::synchronize(const SystemModel& model, std::uint64_t now_ms)
{
    OutputRecord siren{};
    OutputRecord cold_valve{};
    OutputRecord hot_valve{};
    const bool has_siren = model.output_snapshot(1, siren);
    const bool has_cold_valve = model.output_snapshot(2, cold_valve);
    const bool has_hot_valve = model.output_snapshot(3, hot_valve);

    std::scoped_lock lock(mutex_);
    if (backend_ == nullptr) return false;
    if (state_.safety_fault_latched) return false;

    if (state_.maintenance_mode) {
        state_.outputs_enabled = false;
        state_.status = hardware_verified_
            ? PhysicalOutputStatus::FailClosed
            : PhysicalOutputStatus::InvalidHardware;
        return true;
    }

    if (!hardware_verified_) {
        state_.status = PhysicalOutputStatus::InvalidHardware;
        state_.outputs_enabled = false;
        return true;
    }

    if (!readiness_.outputs_allowed() ||
        !commissioning_state_allows_physical_outputs(commissioning_)) {
        if (state_.outputs_enabled ||
            state_.cold_valve.direction != ValveMotionDirection::Stopped ||
            state_.hot_valve.direction != ValveMotionDirection::Stopped) {
            if (!force_safe_locked()) return false;
        }
        state_.status = PhysicalOutputStatus::FailClosed;
        return true;
    }

    LimitSnapshot limits{};
    if (!read_limits_locked(limits)) {
        return latch_fault_locked(PhysicalOutputStatus::BackendError);
    }

    if ((limits.cold_open && limits.cold_closed) ||
        (limits.hot_open && limits.hot_closed)) {
        return latch_fault_locked(PhysicalOutputStatus::ValveSafetyFault);
    }

    state_.outputs_enabled = true;
    state_.status = PhysicalOutputStatus::Ready;

    if (has_siren && siren.commanded && siren.command_revision != siren_command_revision_) {
        if (!write_logical_locked(PhysicalOutputChannel::Siren, siren.active)) {
            return latch_fault_locked(PhysicalOutputStatus::BackendError);
        }
        siren_command_revision_ = siren.command_revision;
    }

    if (!process_valve_locked(
            state_.cold_valve,
            PhysicalOutputChannel::ColdValveOpen,
            PhysicalOutputChannel::ColdValveClose,
            limits.cold_open,
            limits.cold_closed,
            has_cold_valve ? &cold_valve : nullptr,
            commissioning_.cold_valve_travel_timeout_ms,
            now_ms)) {
        return false;
    }

    if (!process_valve_locked(
            state_.hot_valve,
            PhysicalOutputChannel::HotValveOpen,
            PhysicalOutputChannel::HotValveClose,
            limits.hot_open,
            limits.hot_closed,
            has_hot_valve ? &hot_valve : nullptr,
            commissioning_.hot_valve_travel_timeout_ms,
            now_ms)) {
        return false;
    }

    return true;
}

PhysicalOutputRuntimeState PhysicalOutputRuntime::state() const
{
    std::scoped_lock lock(mutex_);
    return state_;
}

const char* to_string(PhysicalOutputStatus status)
{
    switch (status) {
        case PhysicalOutputStatus::Ready: return "ready";
        case PhysicalOutputStatus::FailClosed: return "fail_closed";
        case PhysicalOutputStatus::InvalidHardware: return "invalid_hardware";
        case PhysicalOutputStatus::BackendError: return "backend_error";
        case PhysicalOutputStatus::ValveSafetyFault: return "valve_safety_fault";
        case PhysicalOutputStatus::ValveTimeout: return "valve_timeout";
    }
    return "unknown";
}

const char* to_string(ValveMotionDirection direction)
{
    switch (direction) {
        case ValveMotionDirection::Stopped: return "stopped";
        case ValveMotionDirection::Opening: return "opening";
        case ValveMotionDirection::Closing: return "closing";
    }
    return "unknown";
}

}  // namespace hg
