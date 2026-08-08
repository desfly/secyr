#include "homeguard/commissioning.hpp"

#include <sstream>

namespace hg {

namespace {
void add_check(CommissioningReport& report, const char* name, bool passed, const char* detail) {
    report.checks.push_back({name, passed, detail});
    if (passed) ++report.passed; else ++report.failed;
}
}

CommissioningReport CommissioningEvaluator::evaluate(const CommissioningInput& in) const {
    CommissioningReport report;
    add_check(report, "controller", in.controller_alive, in.controller_alive ? "controller runtime alive" : "controller runtime unavailable");
    add_check(report, "zones", in.zones_available, in.zones_available ? "zone input path available" : "zone input path unavailable");
    add_check(report, "analog", in.analog_available, in.analog_available ? "analog telemetry available" : "analog telemetry unavailable");
    add_check(report, "event_log", in.event_log_available, in.event_log_available ? "event log available" : "event log unavailable");
    add_check(report, "gpio_map", in.gpio_map_verified, in.gpio_map_verified ? "GPIO map verified" : "GPIO map not verified");
    add_check(report, "active_polarity", in.active_polarity_verified, in.active_polarity_verified ? "active polarity verified" : "active polarity not verified");
    add_check(report, "outputs", in.physical_outputs_available, in.physical_outputs_available ? "physical outputs available" : "physical outputs fail-closed");
    add_check(report, "maintenance", in.maintenance_active, in.maintenance_active ? "maintenance mode active" : "maintenance mode required for actuator test");
    add_check(report, "system_disarmed", !in.system_armed, !in.system_armed ? "system disarmed" : "system armed");
    add_check(report, "alarm_clear", !in.alarm_active, !in.alarm_active ? "no active alarm" : "active alarm blocks actuator test");

    const bool dry = in.controller_alive && in.zones_available && in.analog_available && in.event_log_available;
    const bool actuator = dry && in.gpio_map_verified && in.active_polarity_verified && in.physical_outputs_available && in.maintenance_active && !in.system_armed && !in.alarm_active;
    report.state = actuator ? CommissioningState::ActuatorTestReady : (dry ? CommissioningState::DryRunReady : CommissioningState::Blocked);
    return report;
}

const char* to_string(CommissioningState state) noexcept {
    switch (state) {
        case CommissioningState::Blocked: return "blocked";
        case CommissioningState::DryRunReady: return "dry_run_ready";
        case CommissioningState::ActuatorTestReady: return "actuator_test_ready";
    }
    return "blocked";
}

std::string commissioning_json(const CommissioningReport& report) {
    std::ostringstream out;
    out << "{\"state\":\"" << to_string(report.state) << "\",\"passed\":" << report.passed
        << ",\"failed\":" << report.failed << ",\"checks\":[";
    for (std::size_t i = 0; i < report.checks.size(); ++i) {
        if (i) out << ',';
        const auto& check = report.checks[i];
        out << "{\"name\":\"" << check.name << "\",\"passed\":" << (check.passed ? "true" : "false")
            << ",\"detail\":\"" << check.detail << "\"}";
    }
    out << "]}";
    return out.str();
}

}  // namespace hg
