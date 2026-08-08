#include "homeguard/output_command.hpp"

namespace hg {

OutputCommandResult apply_output_command(
    SystemModel& model,
    const BootReadinessReport& readiness,
    const OutputCommand& command)
{
    if (model.output(command.output_id) == nullptr) {
        return {OutputCommandStatus::InvalidOutput, OutputInterlockDecision::InvalidOutput, false};
    }

    const auto interlock = evaluate_output_interlock(
        model,
        OutputInterlockRequest{
            command.output_id,
            command.active,
            command.alarm_active,
            &readiness,
        });

    if (!interlock.allow) {
        const auto* output = model.output(command.output_id);
        return {
            OutputCommandStatus::RejectedInterlock,
            interlock.decision,
            output != nullptr && output->active,
        };
    }

    if (!model.set_output_active(command.output_id, command.active, command.timestamp_ms)) {
        return {OutputCommandStatus::InvalidOutput, OutputInterlockDecision::InvalidOutput, false};
    }

    const auto* output = model.output(command.output_id);
    return {
        OutputCommandStatus::Applied,
        OutputInterlockDecision::Allowed,
        output != nullptr && output->active,
    };
}

const char* to_string(OutputCommandStatus status)
{
    switch (status) {
    case OutputCommandStatus::Applied: return "applied";
    case OutputCommandStatus::RejectedInterlock: return "rejected_interlock";
    case OutputCommandStatus::InvalidOutput: return "invalid_output";
    }
    return "unknown";
}

}  // namespace hg
