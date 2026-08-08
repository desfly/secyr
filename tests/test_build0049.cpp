#include "test_framework.hpp"
#include "homeguard/boot_readiness.hpp"

namespace {

hg::HardwareVerificationRecord valid_hardware() {
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
    return s;
}

}  // namespace

void test_build0049() {
    auto hardware = valid_hardware();
    auto commissioning = valid_commissioning();

    auto report = hg::evaluate_boot_readiness({&hardware, &commissioning});
    CHECK(report.status == hg::BootReadinessStatus::ReadyForPhysicalOutputs);
    CHECK(report.outputs_allowed());
    CHECK(hg::boot_readiness_json(report).find("\"outputsAllowed\":true") != std::string::npos);

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

    auto no_dry_run = commissioning;
    no_dry_run.successful_dry_runs = 0U;
    no_dry_run.successful_actuator_tests = 0U;
    report = hg::evaluate_boot_readiness({&hardware, &no_dry_run});
    CHECK(report.status == hg::BootReadinessStatus::BlockedDryRunRequired);
    CHECK(!report.outputs_allowed());
}
