#pragma once

#include "homeguard/boot_readiness.hpp"
#include "homeguard/output_interlock.hpp"
#include "homeguard/system_model.hpp"

#include <cstdint>

namespace hg {

enum class OutputCommandStatus : std::uint8_t {
    Applied,
    RejectedInterlock,
    InvalidOutput,
};

struct OutputCommand {
    std::uint16_t output_id{};
    bool active{};
    bool alarm_active{};
    std::uint64_t timestamp_ms{};
};

struct OutputCommandResult {
    OutputCommandStatus status{OutputCommandStatus::RejectedInterlock};
    OutputInterlockDecision interlock{OutputInterlockDecision::BootNotReady};
    bool resulting_active{};
};

[[nodiscard]] OutputCommandResult apply_output_command(
    SystemModel& model,
    const BootReadinessReport& readiness,
    const OutputCommand& command);

[[nodiscard]] const char* to_string(OutputCommandStatus status);

}  // namespace hg
