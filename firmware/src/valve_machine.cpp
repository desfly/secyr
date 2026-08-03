#include "homeguard/valve_machine.hpp"

namespace homeguard {

ValveMachine::ValveMachine(std::uint32_t timeout_seconds)
    : timeout_seconds_(timeout_seconds > 0 ? timeout_seconds : 30)
{
}

void ValveMachine::stop_outputs()
{
    snapshot_.outputs = {};
    snapshot_.motion_started_ms = 0;
    snapshot_.deadline_ms = 0;
}

void ValveMachine::begin_motion(
    ValveState state,
    bool open,
    std::uint64_t now_ms)
{
    snapshot_.state = state;
    snapshot_.outputs.drive_open = open;
    snapshot_.outputs.drive_close = !open;
    snapshot_.motion_started_ms = now_ms;
    snapshot_.deadline_ms =
        now_ms + static_cast<std::uint64_t>(
            timeout_seconds_) * 1000ULL;
}

void ValveMachine::command(
    ValveCommand command,
    std::uint64_t now_ms)
{
    switch (command) {
    case ValveCommand::Stop:
        stop_outputs();
        snapshot_.state = ValveState::Unknown;
        break;

    case ValveCommand::Open:
        if (!snapshot_.emergency_latched &&
            snapshot_.state != ValveState::Manual) {
            begin_motion(ValveState::Opening, true, now_ms);
        }
        break;

    case ValveCommand::Close:
        if (snapshot_.state != ValveState::Manual) {
            begin_motion(ValveState::Closing, false, now_ms);
        }
        break;

    case ValveCommand::EmergencyClose:
        snapshot_.emergency_latched = true;
        begin_motion(
            ValveState::EmergencyClosing,
            false,
            now_ms);
        break;

    case ValveCommand::EnterManual:
        stop_outputs();
        snapshot_.state = ValveState::Manual;
        break;

    case ValveCommand::LeaveManual:
        if (snapshot_.state == ValveState::Manual) {
            snapshot_.state = ValveState::Unknown;
        }
        break;
    }
}

ValveSnapshot ValveMachine::update(
    const ValveInputs& inputs,
    std::uint64_t now_ms)
{
    if (inputs.manual_mode &&
        snapshot_.state != ValveState::Manual) {
        command(ValveCommand::EnterManual, now_ms);
    }

    if (snapshot_.state == ValveState::Manual) {
        return snapshot_;
    }

    if (inputs.limit_open && inputs.limit_closed) {
        stop_outputs();
        snapshot_.state = ValveState::Jammed;
        ++snapshot_.fault_count;
        return snapshot_;
    }

    if (inputs.overcurrent &&
        (snapshot_.outputs.drive_open ||
         snapshot_.outputs.drive_close)) {
        stop_outputs();
        snapshot_.state = ValveState::Jammed;
        ++snapshot_.fault_count;
        return snapshot_;
    }

    if ((snapshot_.state == ValveState::Opening) &&
        inputs.limit_open) {
        stop_outputs();
        snapshot_.state = ValveState::Open;
        return snapshot_;
    }

    if ((snapshot_.state == ValveState::Closing ||
         snapshot_.state == ValveState::EmergencyClosing) &&
        inputs.limit_closed) {
        stop_outputs();
        snapshot_.state = ValveState::Closed;
        return snapshot_;
    }

    if (snapshot_.deadline_ms != 0 &&
        now_ms >= snapshot_.deadline_ms) {
        stop_outputs();
        snapshot_.state = ValveState::Timeout;
        ++snapshot_.fault_count;
    }

    return snapshot_;
}

const ValveSnapshot& ValveMachine::snapshot() const noexcept
{
    return snapshot_;
}

void ValveMachine::clear_emergency_latch()
{
    snapshot_.emergency_latched = false;
}

const char* to_string(ValveState state) noexcept
{
    switch (state) {
    case ValveState::Unknown:
        return "unknown";
    case ValveState::Open:
        return "open";
    case ValveState::Closed:
        return "closed";
    case ValveState::Opening:
        return "opening";
    case ValveState::Closing:
        return "closing";
    case ValveState::Jammed:
        return "jammed";
    case ValveState::Timeout:
        return "timeout";
    case ValveState::Manual:
        return "manual";
    case ValveState::EmergencyClosing:
        return "emergency_closing";
    default:
        return "unknown";
    }
}

}  // namespace homeguard
