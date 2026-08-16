#include "test_framework.hpp"
#include "homeguard/boot_readiness.hpp"

namespace {

hg::HardwareVerificationRecord valid_hardware() {
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
    r.pins.siren = hg::gpio_unassigned;
    r.pins.valve1 = hg::gpio_unassigned;
    r.pins.valve2 = hg::gpio_unassigned;
    r.pins.aux1 = hg::gpio_unassigned;
    r.pins.aux2 = hg::gpio_unassigned;
    r.active_polarity_verified = true;
    r.verified_at_ms = 1000U;
    r.profile_crc32 = hg::hardware_profile_crc32(r);
    return r;
}

hg::CommissioningPersistentState valid_commissioning() {
    hg::CommissioningPersistentState s;
    s.gpio_map_verified = true;
    s.active_polarity_verified = true;
    s.successful_dry_runs = 2U;
    s.successful_actuator_tests = 1U;
    s.last_verified_at_ms = 1000U;
    s.valve_limit_polarity_verified = true;
    s.valve_limits_active_low = true;
    s.cold_valve_travel_timeout_ms = 12000U;
    s.hot_valve_travel_timeout_ms = 13000U;
    return s;
}

}  // namespace

void test_build0049() {
    auto hardware = valid_hardware();
    auto commissioning = valid_commissioning();

    CHECK(hg::HardwareVerificationRecord::kSchemaVersion == 2U);
    CHECK(hg::CommissioningPersistentState::kSchemaVersion == 2U);
    CHECK(!hg::gpio_number_valid(22));
    CHECK(!hg::gpio_number_valid(23));
    CHECK(!hg::gpio_number_valid(24));
    CHECK(!hg::gpio_number_valid(25));
    CHECK(hg::gpio_number_valid(21));
    CHECK(hg::gpio_number_valid(26));

    auto report = hg::evaluate_boot_readiness({&hardware, &commissioning});
    CHECK(report.status == hg::BootReadinessStatus::ReadyForPhysicalOutputs);
    CHECK(report.outputs_allowed());
    CHECK(hg::boot_readiness_json(report).find("\"outputsAllowed\":true") != std::string::npos);

    auto legacy_schema = hardware;
    legacy_schema.schema_version = 1U;
    legacy_schema.profile_crc32 = hg::hardware_profile_crc32(legacy_schema);
    CHECK(hg::validate_hardware_verification(legacy_schema) == hg::HardwareVerificationStatus::InvalidSchema);

    auto direct_output = hardware;
    direct_output.pins.siren = 26;
    direct_output.profile_crc32 = hg::hardware_profile_crc32(direct_output);
    CHECK(hg::validate_hardware_verification(direct_output) == hg::HardwareVerificationStatus::InvalidPinMap);

    auto wrong_i2c = hardware;
    wrong_i2c.pins.i2c_sda = 26;
    wrong_i2c.profile_crc32 = hg::hardware_profile_crc32(wrong_i2c);
    CHECK(hg::validate_hardware_verification(wrong_i2c) == hg::HardwareVerificationStatus::InvalidPinMap);

    auto wrong_w5500 = hardware;
    wrong_w5500.pins.w5500_cs = 26;
    wrong_w5500.profile_crc32 = hg::hardware_profile_crc32(wrong_w5500);
    CHECK(hg::validate_hardware_verification(wrong_w5500) == hg::HardwareVerificationStatus::InvalidPinMap);

    report = hg::evaluate_boot_readiness({nullptr, &commissioning});
    CHECK(report.status == hg::BootReadinessStatus::BlockedMissingHardwareRecord);
    CHECK(!report.outputs_allowed());

    report = hg::evaluate_boot_readiness({&hardware, nullptr});
    CHECK(report.status == hg::BootReadinessStatus::BlockedMissingCommissioningState);

    auto bad_hardware = hardware;
    bad_hardware.profile_crc32 ^= 1U;
    report = hg::evaluate_boot_readiness({&bad_hardware, &commissioning});
    CHECK(report.status == hg::BootReadinessStatus::BlockedHardwareVerification);

    auto bad_commissioning = commissioning;
    bad_commissioning.gpio_map_verified = false;
    report = hg::evaluate_boot_readiness({&hardware, &bad_commissioning});
    CHECK(report.status == hg::BootReadinessStatus::BlockedCommissioningState);

    auto missing_valve_safety = commissioning;
    missing_valve_safety.valve_limit_polarity_verified = false;
    report = hg::evaluate_boot_readiness({&hardware, &missing_valve_safety});
    CHECK(report.status == hg::BootReadinessStatus::BlockedCommissioningState);
    CHECK(!report.outputs_allowed());

    auto no_dry_run = commissioning;
    no_dry_run.successful_dry_runs = 0U;
    no_dry_run.successful_actuator_tests = 0U;
    report = hg::evaluate_boot_readiness({&hardware, &no_dry_run});
    CHECK(report.status == hg::BootReadinessStatus::BlockedDryRunRequired);
    CHECK(!report.outputs_allowed());

    auto no_actuator_test = commissioning;
    no_actuator_test.successful_actuator_tests = 0U;
    report = hg::evaluate_boot_readiness({&hardware, &no_actuator_test});
    CHECK(report.status == hg::BootReadinessStatus::BlockedActuatorTestRequired);
    CHECK(!report.outputs_allowed());
    CHECK(hg::boot_readiness_json(report).find("actuator_test_required") != std::string::npos);
}
