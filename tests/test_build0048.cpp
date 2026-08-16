#include "test_framework.hpp"
#include "homeguard/hardware_verification.hpp"

namespace {

hg::HardwareVerificationRecord valid_record() {
    hg::HardwareVerificationRecord r;
    r.pins.i2c_sda = 4;
    r.pins.i2c_scl = 5;
    r.pins.w5500_mosi = 11;
    r.pins.w5500_miso = 13;
    r.pins.w5500_sclk = 12;
    r.pins.w5500_cs = 10;
    r.pins.w5500_int = 9;
    r.pins.w5500_rst = 8;
    r.pins.service_button = 21;
    r.active_polarity_verified = true;
    r.verified_at_ms = 123456U;
    r.profile_crc32 = hg::hardware_profile_crc32(r);
    return r;
}

}  // namespace

void test_build0048() {
    auto r = valid_record();
    CHECK(hg::HardwareVerificationRecord::kSchemaVersion == 2U);
    CHECK(hg::validate_hardware_verification(r) == hg::HardwareVerificationStatus::Valid);
    CHECK(hg::hardware_verification_allows_outputs(r));

    auto duplicate = r;
    duplicate.pins.w5500_mosi = duplicate.pins.w5500_miso;
    duplicate.profile_crc32 = hg::hardware_profile_crc32(duplicate);
    CHECK(hg::validate_hardware_verification(duplicate) == hg::HardwareVerificationStatus::InvalidPinMap);

    auto legacy_direct_output = r;
    legacy_direct_output.pins.valve1 = 26;
    legacy_direct_output.profile_crc32 = hg::hardware_profile_crc32(legacy_direct_output);
    CHECK(hg::validate_hardware_verification(legacy_direct_output) == hg::HardwareVerificationStatus::InvalidPinMap);

    auto nonexistent_gpio = r;
    nonexistent_gpio.pins.service_button = 22;
    nonexistent_gpio.profile_crc32 = hg::hardware_profile_crc32(nonexistent_gpio);
    CHECK(hg::validate_hardware_verification(nonexistent_gpio) == hg::HardwareVerificationStatus::InvalidPinMap);

    auto polarity = r;
    polarity.active_polarity_verified = false;
    polarity.profile_crc32 = hg::hardware_profile_crc32(polarity);
    CHECK(hg::validate_hardware_verification(polarity) == hg::HardwareVerificationStatus::PolarityUnverified);

    auto timestamp = r;
    timestamp.verified_at_ms = 0U;
    timestamp.profile_crc32 = hg::hardware_profile_crc32(timestamp);
    CHECK(hg::validate_hardware_verification(timestamp) == hg::HardwareVerificationStatus::MissingTimestamp);

    auto crc = r;
    crc.profile_crc32 ^= 0x1U;
    CHECK(hg::validate_hardware_verification(crc) == hg::HardwareVerificationStatus::CrcMismatch);

    auto schema = r;
    schema.schema_version = 1U;
    schema.profile_crc32 = hg::hardware_profile_crc32(schema);
    CHECK(hg::validate_hardware_verification(schema) == hg::HardwareVerificationStatus::InvalidSchema);

    const auto json = hg::hardware_verification_json(r);
    CHECK(json.find("\"outputsAllowed\":true") != std::string::npos);
    CHECK(json.find("\"outputBackend\":\"mcp23017_port_a\"") != std::string::npos);
}
