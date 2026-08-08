#include "homeguard/hardware_api.hpp"

#include <sstream>

namespace homeguard {

HardwareApiResponse hardware_status_response(
    const HardwareRuntimeStatus& status)
{
    return {
        200,
        "application/json",
        hardware_runtime_json(status),
    };
}

HardwareApiResponse hardware_test_readiness_response(
    const hg::HardwareReadinessReport& report)
{
    std::ostringstream out;
    out << "{\"dryRunReady\":" << (report.ready_for_dry_run() ? "true" : "false")
        << ",\"actuatorTestReady\":" << (report.ready_for_actuator_test() ? "true" : "false")
        << ",\"checks\":[";
    for (std::size_t i = 0; i < report.items.size(); ++i) {
        if (i != 0) out << ',';
        const auto& item = report.items[i];
        out << "{\"name\":\"" << item.name << "\",\"ready\":" << (item.ready ? "true" : "false")
            << ",\"detail\":\"" << item.detail << "\"}";
    }
    out << "]}";
    return {200, "application/json", out.str()};
}

HardwareApiResponse hardware_test_result_response(
    const hg::HardwareTestResult& result)
{
    const int status = result.allowed() ? 200 : 409;
    std::ostringstream out;
    out << "{\"allowed\":" << (result.allowed() ? "true" : "false")
        << ",\"decision\":\"" << hg::to_string(result.decision) << "\""
        << ",\"target\":\"" << hg::to_string(result.target) << "\""
        << ",\"durationMs\":" << result.requested_duration_ms << '}';
    return {status, "application/json", out.str()};
}

}  // namespace homeguard
