#include "test_framework.hpp"
#include "homeguard/hardware_verification.hpp"

namespace {

hg::HardwareVerificationRecord valid_record() {
    hg::HardwareVerificationRecord r;
    r.pins.i2c_sda = 1;
    r.pins.i2c_scl = 2;
    r.pins.w5500_mosi = 3;
    r.pins.w5500_miso = 4;
    r.pins.w5500_sclk = 5;
    r.pins.w5500_cs = 6;
    r.pins.w5500_int = 7;
    r.pins.w5500_rst = 8;
    r.pins.service_button = 9;
    r.pins.siren = 10;
    r.pins.valve1 = 11;
    r.pins.valve2 = 12;
    r.pins.aux1 = 13;
    r.pins.aux2 = 14;
    r.active_polarity_verified = true;
    r.verified_at_ms = 123456U;
    r.profile_crc32 = hg::hardware_profile_crc32(r);
    return r;
}

}  // namespace

void test_build0048() {
    auto r = valid_record();
    CHECK(hg::validate_hardware_verification(r) == hg::HardwareVerificationStatus::Valid);
    CHECK(hg::hardware_verification_allows_outputs(r));

    auto duplicate = r;
    duplicate.pins.aux2 = duplicate.pins.aux1;
    duplicate.profile_crc32 = hg::hardware_profile_crc32(duplicate);
    CHECK(hg::validate_hardware_verification(duplicate) == hg::HardwareVerificationStatus::InvalidPinMap);

    auto unassigned = r;
    unassigned.pins.valve1 = hg::gpio_unassigned;
    unassigned.profile_crc32 = hg::hardware_profile_crc32(unassigned);
    CHECK(hg::validate_hardware_verification(unassigned) == hg::HardwareVerificationStatus::UnassignedRequiredOutput);

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
    schema.schema_version = 99U;
    schema.profile_crc32 = hg::hardware_profile_crc32(schema);
    CHECK(hg::validate_hardware_verification(schema) == hg::HardwareVerificationStatus::InvalidSchema);

    CHECK(hg::hardware_verification_json(r).find("\"outputsAllowed\":true") != std::string::npos);
}
