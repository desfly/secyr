#include "homeguard/service_readiness.hpp"

#include <sstream>

namespace hg {

ServiceReadinessSnapshot make_service_readiness_snapshot(
    const HardwareVerificationRecord* hardware,
    const CommissioningPersistentState* commissioning)
{
    ServiceReadinessSnapshot result{};
    result.hardware_record_present = hardware != nullptr;
    result.commissioning_record_present = commissioning != nullptr;
    result.hardware_record_valid = hardware != nullptr &&
        validate_hardware_verification(*hardware) == HardwareVerificationStatus::Valid;
    result.commissioning_record_valid = commissioning != nullptr &&
        validate_commissioning_state(*commissioning) == CommissioningStateValidation::Valid;

    // Pass present schema-v2 commissioning progress to BootReadiness even when
    // it is not yet fully Valid. That preserves dry_run_required ->
    // valve_safety_profile_required -> actuator_test_required diagnostics.
    result.boot = evaluate_boot_readiness({
        result.hardware_record_valid ? hardware : nullptr,
        commissioning,
    });
    return result;
}

std::string service_readiness_json(const ServiceReadinessSnapshot& snapshot)
{
    std::ostringstream out;
    out << "{\"status\":\"" << to_string(snapshot.boot.status)
        << "\",\"outputsAllowed\":" << (snapshot.boot.outputs_allowed() ? "true" : "false")
        << ",\"hardwareRecordPresent\":" << (snapshot.hardware_record_present ? "true" : "false")
        << ",\"hardwareRecordValid\":" << (snapshot.hardware_record_valid ? "true" : "false")
        << ",\"commissioningRecordPresent\":" << (snapshot.commissioning_record_present ? "true" : "false")
        << ",\"commissioningRecordValid\":" << (snapshot.commissioning_record_valid ? "true" : "false")
        << '}';
    return out.str();
}

}  // namespace hg
