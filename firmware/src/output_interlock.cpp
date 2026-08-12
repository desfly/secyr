#include "homeguard/output_interlock.hpp"

namespace hg {

OutputInterlockResult evaluate_output_interlock(
    const SystemModel& model,
    const OutputInterlockRequest& request)
{
    if (model.output(request.output_id) == nullptr) {
        return {OutputInterlockDecision::InvalidOutput, false};
    }

    // Deactivation is always permitted once the target exists. This is the
    // fail-safe path and must stay available even while boot/commissioning
    // readiness is not established or an alarm currently owns the outputs.
    if (!request.requested_active) {
        return {OutputInterlockDecision::Allowed, true};
    }

    // Only activation requires the commissioning/readiness gate.
    if (request.readiness == nullptr || !request.readiness->outputs_allowed()) {
        return {OutputInterlockDecision::BootNotReady, false};
    }

    // While alarm handling owns physical outputs, service/manual activation is
    // rejected so two control paths cannot fight over a relay/siren/valve.
    if (request.alarm_active) {
        return {OutputInterlockDecision::AlarmActive, false};
    }

    return {OutputInterlockDecision::Allowed, true};
}

const char* to_string(OutputInterlockDecision decision)
{
    switch (decision) {
    case OutputInterlockDecision::Allowed: return "allowed";
    case OutputInterlockDecision::BootNotReady: return "boot_not_ready";
    case OutputInterlockDecision::AlarmActive: return "alarm_active";
    case OutputInterlockDecision::InvalidOutput: return "invalid_output";
    }
    return "unknown";
}

}  // namespace hg
