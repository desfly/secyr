#include "test_framework.hpp"
#include "homeguard/hardware_profile.hpp"
#include "homeguard/hardware_capabilities.hpp"
#include "homeguard/service_button.hpp"
#include "homeguard/startup_flow.hpp"

void test_build0009() {
    CHECK(hg::HardwareProfile::board == "HW-678 V0.0.0");
    CHECK(hg::HardwareProfile::module == "ESP32-S3-WROOM-1-N16R8");
    CHECK(hg::HardwareProfile::ethernet == "W5500 LAN module");

    hg::BoardPinMap empty{};
    CHECK(hg::validate_pin_map(empty).ok());
    CHECK(!hg::w5500_assigned(empty));
    CHECK(!hg::i2c_assigned(empty));

    const auto empty_capabilities = hg::derive_capabilities(empty);
    CHECK(empty_capabilities.i2c == hg::CapabilityState::Unavailable);
    CHECK(empty_capabilities.w5500 == hg::CapabilityState::Unavailable);
    CHECK(empty_capabilities.service_button == hg::CapabilityState::Unavailable);
    CHECK(empty_capabilities.outputs == hg::CapabilityState::Unavailable);
    CHECK(empty_capabilities.configured_output_count == 0);
    CHECK(empty_capabilities.safe_for_unverified_board());
    CHECK(hg::gpio_number_valid(-1));
    CHECK(hg::gpio_number_valid(48));
    CHECK(!hg::gpio_number_valid(49));

    hg::BoardPinMap partial_i2c{};
    partial_i2c.i2c_sda = 8;
    CHECK(hg::validate_pin_map(partial_i2c).error == hg::PinMapError::IncompleteI2c);

    hg::BoardPinMap duplicate{};
    duplicate.i2c_sda = 8;
    duplicate.i2c_scl = 9;
    duplicate.service_button = 8;
    CHECK(hg::validate_pin_map(duplicate).error == hg::PinMapError::DuplicateGpio);

    hg::BoardPinMap complete{};
    complete.i2c_sda = 1;
    complete.i2c_scl = 2;
    complete.w5500_mosi = 3;
    complete.w5500_miso = 4;
    complete.w5500_sclk = 5;
    complete.w5500_cs = 6;
    complete.w5500_int = 7;
    complete.w5500_rst = 8;
    complete.service_button = 9;
    CHECK(hg::validate_pin_map(complete).ok());
    CHECK(hg::w5500_assigned(complete));
    CHECK(hg::i2c_assigned(complete));

    const auto complete_capabilities = hg::derive_capabilities(complete);
    CHECK(complete_capabilities.i2c == hg::CapabilityState::Configured);
    CHECK(complete_capabilities.w5500 == hg::CapabilityState::Configured);
    CHECK(complete_capabilities.service_button == hg::CapabilityState::Configured);
    CHECK(complete_capabilities.outputs == hg::CapabilityState::Unavailable);
    CHECK(complete_capabilities.safe_for_unverified_board());

    complete.siren = 10;
    complete.valve1 = 11;
    const auto output_capabilities = hg::derive_capabilities(complete);
    CHECK(output_capabilities.outputs == hg::CapabilityState::Configured);
    CHECK(output_capabilities.configured_output_count == 2);
    CHECK(!output_capabilities.safe_for_unverified_board());
    CHECK(hg::to_string(hg::CapabilityState::Unavailable) == "unavailable");
    CHECK(hg::to_string(hg::CapabilityState::Configured) == "configured");

    hg::ServiceButton button({40, 3000, 10000});
    CHECK(button.update(true, 0) == hg::ServiceButtonEvent::None);
    CHECK(button.update(true, 39) == hg::ServiceButtonEvent::None);
    CHECK(button.update(true, 40) == hg::ServiceButtonEvent::None);
    CHECK(button.pressed());
    CHECK(button.update(true, 3039) == hg::ServiceButtonEvent::None);
    CHECK(button.update(true, 3040) == hg::ServiceButtonEvent::ServiceModeRequested);
    CHECK(button.update(true, 5000) == hg::ServiceButtonEvent::None);
    CHECK(button.update(true, 10040) == hg::ServiceButtonEvent::FactoryResetRequested);
    CHECK(button.update(true, 12000) == hg::ServiceButtonEvent::None);
    CHECK(button.update(false, 12001) == hg::ServiceButtonEvent::None);
    CHECK(button.update(false, 12041) == hg::ServiceButtonEvent::None);
    CHECK(!button.pressed());

    hg::StartupFlow flow;
    CHECK(flow.boot(false, true, 0) == hg::StartupAction::StartSetupAp);
    CHECK(flow.state() == hg::StartupState::SetupAp);
    CHECK(flow.provisioning_committed() == hg::StartupAction::RestartController);
    CHECK(flow.state() == hg::StartupState::RestartPending);
    CHECK(flow.boot(true, true, 100) == hg::StartupAction::StartWifiSta);
    CHECK(flow.wifi_connected() == hg::StartupAction::StartLocalServices);
    CHECK(flow.local_services_started() == hg::StartupAction::StartCloud);
    CHECK(flow.state() == hg::StartupState::LocalReady);
    CHECK(flow.cloud_connected() == hg::StartupAction::None);
    CHECK(flow.state() == hg::StartupState::CloudReady);

    hg::StartupFlow retry;
    CHECK(retry.boot(true, false, 0) == hg::StartupAction::StartWifiSta);
    CHECK(retry.wifi_failed(100) == hg::StartupAction::None);
    CHECK(retry.state() == hg::StartupState::Offline);
    CHECK(retry.retry_count() == 1);
    CHECK(retry.tick(1099) == hg::StartupAction::None);
    CHECK(retry.tick(1100) == hg::StartupAction::RetryWifi);
    CHECK(retry.wifi_connected() == hg::StartupAction::StartLocalServices);
    CHECK(retry.local_services_started() == hg::StartupAction::None);
    CHECK(retry.state() == hg::StartupState::LocalReady);
}
