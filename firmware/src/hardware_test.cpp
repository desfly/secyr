#include "homeguard/hardware_test.hpp"

namespace hg {

bool HardwareReadinessReport::ready_for_dry_run() const noexcept {
    for (const auto& item : items) {
        if (item.name != "physical_outputs" && !item.ready) return false;
    }
    return true;
}

bool HardwareReadinessReport::ready_for_actuator_test() const noexcept {
    if (!ready_for_dry_run()) return false;
    for (const auto& item : items) {
        if (item.name == "physical_outputs") return item.ready;
    }
    return false;
}

HardwareTestResult HardwareTestPolicy::evaluate(
    const HardwareTestContext& context,
    const HardwareTestRequest& request) const noexcept {
    HardwareTestResult result{};
    result.target = request.target;
    result.requested_duration_ms = request.duration_ms;
    result.requested_outputs = output_for(request.target);

    if (!context.maintenance_active) {
        result.decision = HardwareTestDecision::BlockedMaintenanceInactive;
    } else if (context.system_armed) {
        result.decision = HardwareTestDecision::BlockedSystemArmed;
    } else if (context.alarm_active) {
        result.decision = HardwareTestDecision::BlockedAlarmActive;
    } else if (!context.physical_outputs_available) {
        result.decision = HardwareTestDecision::BlockedOutputsUnavailable;
    } else if (request.duration_ms == 0 || request.duration_ms > kMaxPulseMs) {
        result.decision = HardwareTestDecision::BlockedInvalidDuration;
    } else {
        result.decision = HardwareTestDecision::Allowed;
    }
    return result;
}

HardwareReadinessReport HardwareTestPolicy::readiness(
    bool controller_alive,
    bool zones_available,
    bool analog_available,
    bool event_log_available,
    bool physical_outputs_available) const {
    return HardwareReadinessReport{{
        {"controller", controller_alive, controller_alive ? "telemetry active" : "no live controller telemetry"},
        {"zones", zones_available, zones_available ? "zone inputs available" : "zone inputs unavailable"},
        {"analog", analog_available, analog_available ? "analog channels available" : "analog channels unavailable"},
        {"event_log", event_log_available, event_log_available ? "event log active" : "event log unavailable"},
        {"physical_outputs", physical_outputs_available,
            physical_outputs_available ? "verified output map enabled" : "GPIO/polarity unverified: actuator tests locked"},
    }};
}

HardwareTestOutputs HardwareTestPolicy::output_for(HardwareTestTarget target) noexcept {
    HardwareTestOutputs outputs{};
    switch (target) {
        case HardwareTestTarget::Siren: outputs.siren = true; break;
        case HardwareTestTarget::Valve1: outputs.valve1 = true; break;
        case HardwareTestTarget::Valve2: outputs.valve2 = true; break;
        case HardwareTestTarget::Aux1: outputs.aux1 = true; break;
        case HardwareTestTarget::Aux2: outputs.aux2 = true; break;
    }
    return outputs;
}

const char* to_string(HardwareTestTarget target) noexcept {
    switch (target) {
        case HardwareTestTarget::Siren: return "siren";
        case HardwareTestTarget::Valve1: return "valve1";
        case HardwareTestTarget::Valve2: return "valve2";
        case HardwareTestTarget::Aux1: return "aux1";
        case HardwareTestTarget::Aux2: return "aux2";
    }
    return "unknown";
}

const char* to_string(HardwareTestDecision decision) noexcept {
    switch (decision) {
        case HardwareTestDecision::Allowed: return "allowed";
        case HardwareTestDecision::BlockedMaintenanceInactive: return "maintenance_inactive";
        case HardwareTestDecision::BlockedSystemArmed: return "system_armed";
        case HardwareTestDecision::BlockedAlarmActive: return "alarm_active";
        case HardwareTestDecision::BlockedOutputsUnavailable: return "outputs_unavailable";
        case HardwareTestDecision::BlockedInvalidDuration: return "invalid_duration";
    }
    return "unknown";
}

}  // namespace hg