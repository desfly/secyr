#pragma once

#include <cstdint>

namespace homeguard {

enum class ValveState {
    Unknown,
    Open,
    Closed,
    Opening,
    Closing,
    Jammed,
    Timeout,
    Manual,
    EmergencyClosing,
};

enum class ValveCommand {
    Stop,
    Open,
    Close,
    EmergencyClose,
    EnterManual,
    LeaveManual,
};

struct ValveInputs {
    bool limit_open{false};
    bool limit_closed{false};
    bool overcurrent{false};
    bool manual_mode{false};
};

struct ValveOutputs {
    bool drive_open{false};
    bool drive_close{false};
};

struct ValveSnapshot {
    ValveState state{ValveState::Unknown};
    ValveOutputs outputs{};
    std::uint64_t motion_started_ms{0};
    std::uint64_t deadline_ms{0};
    std::uint32_t fault_count{0};
    bool emergency_latched{false};
};

class ValveMachine {
public:
    explicit ValveMachine(std::uint32_t timeout_seconds = 30);

    void command(
        ValveCommand command,
        std::uint64_t now_ms);

    ValveSnapshot update(
        const ValveInputs& inputs,
        std::uint64_t now_ms);

    const ValveSnapshot& snapshot() const noexcept;
    void clear_emergency_latch();

private:
    void stop_outputs();
    void begin_motion(
        ValveState state,
        bool open,
        std::uint64_t now_ms);

    std::uint32_t timeout_seconds_;
    ValveSnapshot snapshot_{};
};

const char* to_string(ValveState state) noexcept;

}  // namespace homeguard
