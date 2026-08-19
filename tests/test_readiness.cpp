#include "test_framework.hpp"
#include "homeguard/boot_readiness.hpp"
#include "homeguard/commissioning_state.hpp"
#include "homeguard/hardware_verification.hpp"
#include "homeguard/service_readiness.hpp"

#include <string>

namespace {
hg::HardwareVerificationRecord valid_hardware() {
    hg::HardwareVerificationRecord r{};
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
    hg::CommissioningPersistentState s{};
    s.gpio_map_verified = true;
    s.active_polarity_verified = true;
    s.successful_dry_runs = 2U;
    s.successful_actuator_tests = 1U;
    s.last_verified_at_ms = 1000U;
    return s;
}

void test_commissioning_state() {
    hg::CommissioningPersistentState state{};
    CHECK(hg::validate_commissioning_state(state) == hg::CommissioningStateValidation::VerificationIncomplete);
    CHECK(!hg::commissioning_state_allows_physical_outputs(state));

    state.gpio_map_verified = true;
    state.active_polarity_verified = true;
    CHECK(hg::validate_commissioning_state(state) == hg::CommissioningStateValidation::Valid);
    CHECK(!hg::commissioning_state_allows_physical_outputs(state));

    state.successful_dry_runs = 1;
    state.last_verified_at_ms = 123456;
    CHECK(hg::commissioning_state_allows_physical_outputs(state));

    state.successful_actuator_tests = 2;
    CHECK(hg::validate_commissioning_state(state) == hg::CommissioningStateValidation::InvalidSequence);
    CHECK(!hg::commissioning_state_allows_physical_outputs(state));

    state.successful_actuator_tests = 1;
    state.schema_version = 99;
    CHECK(hg::validate_commissioning_state(state) == hg::CommissioningStateValidation::InvalidSchema);

    state.schema_version = hg::CommissioningPersistentState::kSchemaVersion;
    const std::string json = hg::commissioning_state_json(state);
    CHECK(json.find("\"gpioMapVerified\":true") != std::string::npos);
    CHECK(json.find("\"physicalOutputsAllowed\":true") != std::string::npos);
}

void test_hardware_verification() {
    auto r = valid_hardware();
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

void test_boot_readiness() {
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

void test_service_readiness() {
    const auto hardware = valid_hardware();
    auto commissioning = valid_commissioning();
    commissioning.successful_dry_runs = 1U;
    commissioning.successful_actuator_tests = 0U;

    const auto ready = hg::make_service_readiness_snapshot(&hardware, &commissioning);
    CHECK(ready.hardware_record_present);
    CHECK(ready.hardware_record_valid);
    CHECK(ready.commissioning_record_present);
    CHECK(ready.commissioning_record_valid);
    CHECK(ready.boot.outputs_allowed());
    CHECK(hg::service_readiness_json(ready).find("\"outputsAllowed\":true") != std::string::npos);

    const auto missing_hw = hg::make_service_readiness_snapshot(nullptr, &commissioning);
    CHECK(!missing_hw.boot.outputs_allowed());
    CHECK(!missing_hw.hardware_record_present);

    auto corrupt = hardware;
    corrupt.profile_crc32 ^= 1U;
    const auto corrupt_hw = hg::make_service_readiness_snapshot(&corrupt, &commissioning);
    CHECK(corrupt_hw.hardware_record_present);
    CHECK(!corrupt_hw.hardware_record_valid);
    CHECK(!corrupt_hw.boot.outputs_allowed());

    auto incomplete = commissioning;
    incomplete.active_polarity_verified = false;
    const auto incomplete_state = hg::make_service_readiness_snapshot(&hardware, &incomplete);
    CHECK(incomplete_state.commissioning_record_present);
    CHECK(!incomplete_state.commissioning_record_valid);
    CHECK(!incomplete_state.boot.outputs_allowed());
}
} // namespace

void test_readiness() {
    test_commissioning_state();
    test_hardware_verification();
    test_boot_readiness();
    test_service_readiness();
}
