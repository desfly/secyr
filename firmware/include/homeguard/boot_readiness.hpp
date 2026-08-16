#pragma once

#include "homeguard/commissioning_state.hpp"
#include "homeguard/hardware_verification.hpp"

#include <string>

namespace hg {

enum class BootReadinessStatus {
    BlockedMissingHardwareRecord,
    BlockedMissingCommissioningState,
    BlockedHardwareVerification,
    BlockedCommissioningState,
    BlockedValveSafetyProfileRequired,
    BlockedDryRunRequired,
    BlockedActuatorTestRequired,
    ReadyForPhysicalOutputs,
};

struct BootReadinessInput {
    const HardwareVerificationRecord* hardware{};
    const CommissioningPersistentState* commissioning{};
};

struct BootReadinessReport {
    BootReadinessStatus status{BootReadinessStatus::BlockedMissingHardwareRecord};
    HardwareVerificationStatus hardware_status{HardwareVerificationStatus::InvalidSchema};
    CommissioningStateValidation commissioning_status{CommissioningStateValidation::InvalidSchema};

    [[nodiscard]] bool outputs_allowed() const noexcept {
        return status == BootReadinessStatus::ReadyForPhysicalOutputs;
    }
};

[[nodiscard]] BootReadinessReport evaluate_boot_readiness(const BootReadinessInput& input) noexcept;
const char* to_string(BootReadinessStatus status) noexcept;
std::string boot_readiness_json(const BootReadinessReport& report);

}  // namespace hg
