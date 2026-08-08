#include "homeguard/hardware_verification.hpp"
#include "homeguard/crc32.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <vector>

namespace hg {
namespace {

void append_u32(std::vector<std::byte>& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        out.push_back(static_cast<std::byte>((value >> static_cast<unsigned>(shift)) & 0xffU));
    }
}

void append_u64(std::vector<std::byte>& out, std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) {
        out.push_back(static_cast<std::byte>((value >> static_cast<unsigned>(shift)) & 0xffULL));
    }
}

void append_i32(std::vector<std::byte>& out, int value) {
    append_u32(out, static_cast<std::uint32_t>(static_cast<std::int32_t>(value)));
}

bool required_outputs_assigned(const BoardPinMap& pins) {
    return pins.siren != gpio_unassigned &&
        pins.valve1 != gpio_unassigned &&
        pins.valve2 != gpio_unassigned &&
        pins.aux1 != gpio_unassigned &&
        pins.aux2 != gpio_unassigned;
}

}  // namespace

std::uint32_t hardware_profile_crc32(const HardwareVerificationRecord& record) noexcept {
    std::vector<std::byte> bytes;
    bytes.reserve(4U + (14U * 4U) + 1U + 8U);
    append_u32(bytes, record.schema_version);
    append_i32(bytes, record.pins.i2c_sda);
    append_i32(bytes, record.pins.i2c_scl);
    append_i32(bytes, record.pins.w5500_mosi);
    append_i32(bytes, record.pins.w5500_miso);
    append_i32(bytes, record.pins.w5500_sclk);
    append_i32(bytes, record.pins.w5500_cs);
    append_i32(bytes, record.pins.w5500_int);
    append_i32(bytes, record.pins.w5500_rst);
    append_i32(bytes, record.pins.service_button);
    append_i32(bytes, record.pins.siren);
    append_i32(bytes, record.pins.valve1);
    append_i32(bytes, record.pins.valve2);
    append_i32(bytes, record.pins.aux1);
    append_i32(bytes, record.pins.aux2);
    bytes.push_back(record.active_polarity_verified ? std::byte{1} : std::byte{0});
    append_u64(bytes, record.verified_at_ms);
    return crc32(bytes);
}

HardwareVerificationStatus validate_hardware_verification(
    const HardwareVerificationRecord& record) noexcept {
    if (record.schema_version != HardwareVerificationRecord::kSchemaVersion) {
        return HardwareVerificationStatus::InvalidSchema;
    }
    if (!validate_pin_map(record.pins).ok()) {
        return HardwareVerificationStatus::InvalidPinMap;
    }
    if (!required_outputs_assigned(record.pins)) {
        return HardwareVerificationStatus::UnassignedRequiredOutput;
    }
    if (!record.active_polarity_verified) {
        return HardwareVerificationStatus::PolarityUnverified;
    }
    if (record.verified_at_ms == 0U) {
        return HardwareVerificationStatus::MissingTimestamp;
    }
    if (record.profile_crc32 != hardware_profile_crc32(record)) {
        return HardwareVerificationStatus::CrcMismatch;
    }
    return HardwareVerificationStatus::Valid;
}

bool hardware_verification_allows_outputs(const HardwareVerificationRecord& record) noexcept {
    return validate_hardware_verification(record) == HardwareVerificationStatus::Valid;
}

std::string hardware_verification_json(const HardwareVerificationRecord& record) {
    const auto status = validate_hardware_verification(record);
    std::ostringstream out;
    out << "{\"status\":\"" << to_string(status)
        << "\",\"schemaVersion\":" << record.schema_version
        << ",\"polarityVerified\":" << (record.active_polarity_verified ? "true" : "false")
        << ",\"verifiedAtMs\":" << record.verified_at_ms
        << ",\"profileCrc32\":" << record.profile_crc32
        << ",\"outputsAllowed\":" << (hardware_verification_allows_outputs(record) ? "true" : "false")
        << "}";
    return out.str();
}

const char* to_string(const HardwareVerificationStatus status) noexcept {
    switch (status) {
        case HardwareVerificationStatus::Valid: return "valid";
        case HardwareVerificationStatus::InvalidSchema: return "invalid_schema";
        case HardwareVerificationStatus::InvalidPinMap: return "invalid_pin_map";
        case HardwareVerificationStatus::UnassignedRequiredOutput: return "unassigned_required_output";
        case HardwareVerificationStatus::PolarityUnverified: return "polarity_unverified";
        case HardwareVerificationStatus::MissingTimestamp: return "missing_timestamp";
        case HardwareVerificationStatus::CrcMismatch: return "crc_mismatch";
    }
    return "unknown";
}

}  // namespace hg
