#pragma once

#include "homeguard/boot_readiness.hpp"
#include "homeguard/commissioning_state.hpp"
#include "homeguard/hardware_verification.hpp"

#include <string>

namespace hg {

struct ServiceReadinessSnapshot {
    BootReadinessReport boot{};
    bool hardware_record_present{};
    bool commissioning_record_present{};
    bool hardware_record_valid{};
    bool commissioning_record_valid{};
};

[[nodiscard]] ServiceReadinessSnapshot make_service_readiness_snapshot(
    const HardwareVerificationRecord* hardware,
    const CommissioningPersistentState* commissioning);

[[nodiscard]] std::string service_readiness_json(const ServiceReadinessSnapshot& snapshot);

}  // namespace hg
