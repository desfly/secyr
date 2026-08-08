#pragma once

#include "homeguard/hardware_profile.hpp"

#include <cstdint>
#include <string>

namespace hg {

struct HardwareVerificationRecord {
    static constexpr std::uint32_t kSchemaVersion = 1;

    std::uint32_t schema_version{kSchemaVersion};
    BoardPinMap pins{};
    bool active_polarity_verified{};
    std::uint64_t verified_at_ms{};
    std::uint32_t profile_crc32{};
};

enum class HardwareVerificationStatus {
    Valid,
    InvalidSchema,
    InvalidPinMap,
    UnassignedRequiredOutput,
    PolarityUnverified,
    MissingTimestamp,
    CrcMismatch,
};

std::uint32_t hardware_profile_crc32(const HardwareVerificationRecord& record) noexcept;
HardwareVerificationStatus validate_hardware_verification(
    const HardwareVerificationRecord& record) noexcept;
bool hardware_verification_allows_outputs(const HardwareVerificationRecord& record) noexcept;
std::string hardware_verification_json(const HardwareVerificationRecord& record);
const char* to_string(HardwareVerificationStatus status) noexcept;

}  // namespace hg
