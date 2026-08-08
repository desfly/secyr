#pragma once

#include "homeguard/boot_readiness.hpp"
#include "homeguard/system_model.hpp"

#include <cstdint>

namespace hg {

enum class OutputInterlockDecision : std::uint8_t {
    Allowed,
    BootNotReady,
    AlarmActive,
    InvalidOutput,
};

struct OutputInterlockRequest {
    std::uint16_t output_id{};
    bool requested_active{};
    bool alarm_active{};
    const BootReadinessReport* readiness{};
};

struct OutputInterlockResult {
    OutputInterlockDecision decision{OutputInterlockDecision::BootNotReady};
    bool allow{};
};

[[nodiscard]] OutputInterlockResult evaluate_output_interlock(
    const SystemModel& model,
    const OutputInterlockRequest& request);

[[nodiscard]] const char* to_string(OutputInterlockDecision decision);

}  // namespace hg
