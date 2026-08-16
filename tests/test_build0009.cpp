#include "test_framework.hpp"
#include "homeguard/hardware_profile.hpp"
#include "homeguard/hardware_capabilities.hpp"
#include "homeguard/hardware_runtime.hpp"
#include "homeguard/service_button.hpp"
#include "homeguard/startup_flow.hpp"

#include <string>

void test_build0009() {
    CHECK(hg::HardwareProfile::board == "HW-678 V0.0.0");
    CHECK(hg::HardwareProfile::module == "ESP32-S3-WROOM-1-N16R8");
    CHECK(hg::HardwareProfile::ethernet == "W5500 LAN module");

    hg::BoardPinMap empty{};
    CHECK(!hg::validate_pin_map(empty).ok());
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
    CHECK(hg::gpio_number_valid(21));
    CHECK(!hg::gpio_number_valid(22));
    CHECK(!hg::gpio_number_valid(25));
    CHECK(hg::gpio_number_valid(26));
    CHECK(hg::gpio_number_valid(48));
    CHECK(!hg::gpio_number_valid(49));

    hg::BoardPinMap partial_i2c{};
    partial_i2c.i2c_sda = 4;
    CHECK(hg::validate_pin_map(partial_i2c).error == hg::PinMapError::IncompleteI2c);

    hg::BoardPinMap complete{};
    complete.i2c_sda = 4;
    complete.i2c_scl = 5;
    complete.w5500_mosi = 11;
    complete.w5500_miso = 13;
    complete.w5500_sclk = 12;
    complete.w5500_cs = 10;
    complete.w5500_int = 9;
    complete.w5500_rst = 8;
    complete.service_button = 21;
    CHECK(hg::validate_pin_map(complete).ok());
    CHECK(hg::w5500_assigned(complete));
    CHECK(hg::i2c_assigned(complete));

    const auto complete_capabilities = hg::derive_capabilities(complete);
    CHECK(complete_capabilities.i2c == hg::CapabilityState::Configured);
    CHECK(complete_capabilities.w5500 == hg::CapabilityState::Configured);
    CHECK(complete_capabilities.service_button == hg::CapabilityState::Configured);
    CHECK(complete_capabilities.outputs == hg::CapabilityState::Unavailable);
    CHECK(complete_capabilities.configured_output_count == 0);
    CHECK(complete_capabilities.safe_for_unverified_board());

    auto wrong_board_map = complete;
    wrong_board_map.i2c_sda = 26;
    CHECK(!hg::validate_pin_map(wrong_board_map).ok());

    auto legacy_direct_output = complete;
    legacy_direct_output.siren = 26;
    CHECK(!hg::validate_pin_map(legacy_direct_output).ok());
    const auto output_capabilities = hg::derive_capabilities(legacy_direct_output);
    CHECK(output_capabilities.outputs == hg::CapabilityState::Unavailable);
    CHECK(output_capabilities.configured_output_count == 0);
    CHECK(output_capabilities.safe_for_unverified_board());
    CHECK(hg::to_string(hg::CapabilityState::Unavailable) == "unavailable");
    CHECK(hg::to_string(hg::CapabilityState::Configured) == "configured");

    // Bootstrap health is explicit and machine-readable. Optional hardware may
    // degrade the controller without being misreported as a total failure.
    homeguard::HardwareRuntimeStatus runtime{};
    CHECK(runtime.overall == homeguard::HardwareBootstrapState::NotInitialized);
    runtime.overall = homeguard::HardwareBootstrapState::Degraded;
    runtime.i2c.state = homeguard::HardwareModuleState::Ready;
    runtime.micro_sd.state = homeguard::HardwareModuleState::Missing;
    const std::string degraded_json = homeguard::hardware_runtime_json(runtime);
    CHECK(degraded_json.find("\"overall\":\"degraded\"") != std::string::npos);
    CHECK(degraded_json.find("\"micro_sd\":{\"state\":\"missing\"") != std::string::npos);
    CHECK(std::string{homeguard::to_string(homeguard::HardwareBootstrapState::Failed)} == "failed");

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
