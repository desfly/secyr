#include "homeguard/boot_readiness.hpp"

#include <sstream>

namespace hg {

BootReadinessReport evaluate_boot_readiness(const BootReadinessInput& input) noexcept {
    BootReadinessReport report;
    if (input.hardware == nullptr) {
        report.status = BootReadinessStatus::BlockedMissingHardwareRecord;
        return report;
    }
    if (input.commissioning == nullptr) {
        report.status = BootReadinessStatus::BlockedMissingCommissioningState;
        return report;
    }

    report.hardware_status = validate_hardware_verification(*input.hardware);
    if (report.hardware_status != HardwareVerificationStatus::Valid) {
        report.status = BootReadinessStatus::BlockedHardwareVerification;
        return report;
    }

    report.commissioning_status = validate_commissioning_state(*input.commissioning);
    if (report.commissioning_status != CommissioningStateValidation::Valid) {
        report.status = BootReadinessStatus::BlockedCommissioningState;
        return report;
    }

    if (input.commissioning->successful_dry_runs == 0U) {
        report.status = BootReadinessStatus::BlockedDryRunRequired;
        return report;
    }

    if (!hardware_verification_allows_outputs(*input.hardware) ||
        !commissioning_state_allows_physical_outputs(*input.commissioning)) {
        report.status = BootReadinessStatus::BlockedCommissioningState;
        return report;
    }

    report.status = BootReadinessStatus::ReadyForPhysicalOutputs;
    return report;
}

const char* to_string(const BootReadinessStatus status) noexcept {
    switch (status) {
        case BootReadinessStatus::BlockedMissingHardwareRecord: return "missing_hardware_record";
        case BootReadinessStatus::BlockedMissingCommissioningState: return "missing_commissioning_state";
        case BootReadinessStatus::BlockedHardwareVerification: return "hardware_verification_failed";
        case BootReadinessStatus::BlockedCommissioningState: return "commissioning_state_failed";
        case BootReadinessStatus::BlockedDryRunRequired: return "dry_run_required";
        case BootReadinessStatus::ReadyForPhysicalOutputs: return "ready_for_physical_outputs";
    }
    return "unknown";
}

std::string boot_readiness_json(const BootReadinessReport& report) {
    std::ostringstream out;
    out << "{\"status\":\"" << to_string(report.status)
        << "\",\"outputsAllowed\":" << (report.outputs_allowed() ? "true" : "false")
        << ",\"hardwareStatus\":\"" << to_string(report.hardware_status)
        << "\",\"commissioningStatus\":\"" << to_string(report.commissioning_status)
        << "\"}";
    return out.str();
}

}  // namespace hg
