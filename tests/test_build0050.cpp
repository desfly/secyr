#include "test_framework.hpp"
#include "homeguard/service_readiness.hpp"

namespace {

hg::HardwareVerificationRecord verified_hardware() {
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
    r.verified_at_ms = 1000;
    r.profile_crc32 = hg::hardware_profile_crc32(r);
    return r;
}

hg::CommissioningPersistentState verified_commissioning() {
    hg::CommissioningPersistentState s;
    s.gpio_map_verified = true;
    s.active_polarity_verified = true;
    s.successful_dry_runs = 1;
    s.successful_actuator_tests = 1;
    s.last_verified_at_ms = 1000;
    return s;
}

}  // namespace

void test_build0050() {
    const auto hw = verified_hardware();
    const auto commissioning = verified_commissioning();

    const auto ready = hg::make_service_readiness_snapshot(&hw, &commissioning);
    CHECK(ready.hardware_record_present);
    CHECK(ready.hardware_record_valid);
    CHECK(ready.commissioning_record_present);
    CHECK(ready.commissioning_record_valid);
    CHECK(ready.boot.outputs_allowed());
    CHECK(hg::service_readiness_json(ready).find("\"outputsAllowed\":true") != std::string::npos);

    auto dry_only = commissioning;
    dry_only.successful_actuator_tests = 0;
    const auto actuator_required = hg::make_service_readiness_snapshot(&hw, &dry_only);
    CHECK(actuator_required.commissioning_record_valid);
    CHECK(!actuator_required.boot.outputs_allowed());
    CHECK(actuator_required.boot.status == hg::BootReadinessStatus::BlockedActuatorTestRequired);

    const auto missing_hw = hg::make_service_readiness_snapshot(nullptr, &commissioning);
    CHECK(!missing_hw.boot.outputs_allowed());
    CHECK(!missing_hw.hardware_record_present);

    auto corrupt = hw;
    corrupt.profile_crc32 ^= 1U;
    const auto corrupt_hw = hg::make_service_readiness_snapshot(&corrupt, &commissioning);
    CHECK(corrupt_hw.hardware_record_present);
    CHECK(!corrupt_hw.hardware_record_valid);
    CHECK(!corrupt_hw.boot.outputs_allowed());

    auto incomplete = commissioning;
    incomplete.active_polarity_verified = false;
    const auto incomplete_state = hg::make_service_readiness_snapshot(&hw, &incomplete);
    CHECK(incomplete_state.commissioning_record_present);
    CHECK(!incomplete_state.commissioning_record_valid);
    CHECK(!incomplete_state.boot.outputs_allowed());
}
