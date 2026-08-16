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

}  // namespace

bool PhysicalOutputRuntime::initialize(
    PhysicalOutputBackend& backend,
    const HardwareVerificationRecord& hardware,
    const CommissioningPersistentState& commissioning,
    const BootReadinessReport& readiness)
{
    std::scoped_lock lock(mutex_);
    backend_ = &backend;
    commissioning_ = &commissioning;
    state_ = {};
    siren_known_ = true;
    siren_active_ = false;

    if (!hardware_verification_allows_outputs(hardware)) {
        state_.status = PhysicalOutputStatus::InvalidHardware;
        return false;
    }

    // HW-678 actuator outputs live on MCP23017 Port A. Configure every logical
    // channel OFF before the readiness gate can ever expose physical control.
    for (const auto channel : kAllChannels) {
        if (!backend_->configure_output(channel_number(channel), false)) {
            ++state_.failures;
            state_.status = PhysicalOutputStatus::BackendError;
            state_.safety_fault_latched = true;
            return false;
        }
    }

    if (!readiness.outputs_allowed() ||
        !commissioning_state_allows_physical_outputs(commissioning)) {
        state_.status = PhysicalOutputStatus::FailClosed;
        state_.outputs_enabled = false;
        return true;
    }

    state_.outputs_enabled = true;
    state_.status = PhysicalOutputStatus::Ready;
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
    siren_known_ = true;
    siren_active_ = false;

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
    if (backend_ == nullptr || commissioning_ == nullptr ||
        !commissioning_->valve_limit_polarity_verified) {
        return false;
    }

    std::uint8_t raw = 0;
    if (!backend_->read_inputs(&raw)) {
        ++state_.failures;
        return false;
    }

    const bool active_low = commissioning_->valve_limits_active_low;
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

    // A new explicit command cancels any in-progress direction first.
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
        // Already at the requested end position: consume this command without
        // energizing the actuator. Both lines remain STOP/OFF.
        ++state_.limit_stops;
        return true;
    }

    // Break-before-make in the core; MCP backend also atomically clears the
    // opposite direction in the same OLAT byte.
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

bool PhysicalOutputRuntime::force_safe()
{
    std::scoped_lock lock(mutex_);
    const bool ok = force_safe_locked();
    if (ok && !state_.safety_fault_latched) {
        state_.status = PhysicalOutputStatus::FailClosed;
    }
    return ok;
}

bool PhysicalOutputRuntime::synchronize(
    const SystemModel& model,
    const BootReadinessReport& readiness,
    std::uint64_t now_ms)
{
    std::scoped_lock lock(mutex_);
    if (backend_ == nullptr || commissioning_ == nullptr) return false;

    if (!readiness.outputs_allowed() ||
        !commissioning_state_allows_physical_outputs(*commissioning_)) {
        if (state_.outputs_enabled ||
            state_.cold_valve.direction != ValveMotionDirection::Stopped ||
            state_.hot_valve.direction != ValveMotionDirection::Stopped ||
            siren_active_) {
            if (!force_safe_locked()) return false;
        }
        if (!state_.safety_fault_latched) state_.status = PhysicalOutputStatus::FailClosed;
        return !state_.safety_fault_latched;
    }

    if (state_.safety_fault_latched) {
        return false;
    }

    LimitSnapshot limits{};
    if (!read_limits_locked(limits)) {
        return latch_fault_locked(PhysicalOutputStatus::BackendError);
    }

    // Both end switches active simultaneously is physically contradictory and
    // must latch the whole actuator runtime fail-closed until service/reboot.
    if ((limits.cold_open && limits.cold_closed) ||
        (limits.hot_open && limits.hot_closed)) {
        return latch_fault_locked(PhysicalOutputStatus::ValveSafetyFault);
    }

    state_.outputs_enabled = true;
    state_.status = PhysicalOutputStatus::Ready;

    const auto* siren = model.output(1);
    const bool requested_siren = siren != nullptr && siren->active;
    if (!siren_known_ || requested_siren != siren_active_) {
        if (!write_logical_locked(PhysicalOutputChannel::Siren, requested_siren)) {
            return latch_fault_locked(PhysicalOutputStatus::BackendError);
        }
        siren_known_ = true;
        siren_active_ = requested_siren;
    }

    if (!process_valve_locked(
            state_.cold_valve,
            PhysicalOutputChannel::ColdValveOpen,
            PhysicalOutputChannel::ColdValveClose,
            limits.cold_open,
            limits.cold_closed,
            model.output(2),
            commissioning_->cold_valve_travel_timeout_ms,
            now_ms)) {
        return false;
    }

    if (!process_valve_locked(
            state_.hot_valve,
            PhysicalOutputChannel::HotValveOpen,
            PhysicalOutputChannel::HotValveClose,
            limits.hot_open,
            limits.hot_closed,
            model.output(3),
            commissioning_->hot_valve_travel_timeout_ms,
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
