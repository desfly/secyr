#include "test_framework.hpp"

#include "homeguard/hardware_api.hpp"
#include "homeguard/hardware_calibration.hpp"
#include "homeguard/hardware_runtime.hpp"
#include "homeguard/infrastructure_status.hpp"
#include "homeguard/modbus_request.hpp"

#include <cmath>
#include <cstdint>
#include <string>

void test_runtime_support()
{
    homeguard::HardwareCalibration calibration;
    CHECK(homeguard::validate_hardware_calibration(calibration));

    const auto& pressure = calibration.pressure[0];
    CHECK(std::fabs(homeguard::pressure_from_millivolts(480.0F, pressure) - 0.0F) < 0.001F);
    CHECK(std::fabs(homeguard::pressure_from_millivolts(1440.0F, pressure) - 5.0F) < 0.001F);
    CHECK(std::fabs(homeguard::pressure_from_millivolts(2400.0F, pressure) - 10.0F) < 0.001F);
    calibration.zones[0].normal_min_mv = 100.0F;
    CHECK(!homeguard::validate_hardware_calibration(calibration));

    homeguard::HardwareRuntimeStatus hardware;
    hardware.i2c = {homeguard::HardwareModuleState::Ready, "GPIO4/GPIO5", 0};
    hardware.ads1115_zones = {homeguard::HardwareModuleState::Ready, "0x48", 0};
    hardware.ads1115_telemetry = {homeguard::HardwareModuleState::Missing, "0x49 missing", 1};
    hardware.mcp23017 = {homeguard::HardwareModuleState::Ready, "outputs off", 0};
    hardware.safe_outputs_forced = true;

    const auto hardware_response = homeguard::hardware_status_response(hardware);
    CHECK(hardware_response.http_status == 200);
    CHECK(hardware_response.body.find("\"i2c\"") != std::string::npos);
    CHECK(hardware_response.body.find("\"state\":\"ready\"") != std::string::npos);
    CHECK(hardware_response.body.find("\"state\":\"missing\"") != std::string::npos);
    CHECK(hardware_response.body.find("\"safe_outputs_forced\":true") != std::string::npos);

    homeguard::InfrastructureStatus infrastructure;
    infrastructure.rtc_ready = true;
    infrastructure.rtc_iso8601 = "2026-08-03T15:00:00+03:00";
    infrastructure.rtc_temperature_c = 29.25F;
    infrastructure.ethernet_initialized = true;
    infrastructure.ethernet_link_up = true;
    infrastructure.ethernet_has_ip = true;
    infrastructure.ethernet_ipv4 = "192.168.1.50";
    infrastructure.sd_mounted = true;
    infrastructure.sd_total_bytes = 32000000000ULL;
    infrastructure.sd_free_bytes = 30000000000ULL;
    infrastructure.battery_monitor_ready = true;
    infrastructure.battery_voltage_v = 12.4F;
    infrastructure.battery_current_a = -0.3F;
    infrastructure.battery_power_w = -3.72F;

    const auto infrastructure_json = homeguard::infrastructure_status_json(infrastructure);
    CHECK(infrastructure_json.find("\"ready\":true") != std::string::npos);
    CHECK(infrastructure_json.find("192.168.1.50") != std::string::npos);
    CHECK(infrastructure_json.find("\"32000000000\"") != std::string::npos);
    CHECK(infrastructure_json.find("\"power_w\":-3.72") != std::string::npos);

    const auto request = homeguard::modbus_read_holding_registers(1, 0, 2);
    CHECK(request[0] == 1);
    CHECK(request[1] == 3);
    CHECK(request[4] == 0);
    CHECK(request[5] == 2);
    const auto crc = homeguard::modbus_crc16_portable(request.data(), 6);
    CHECK(request[6] == static_cast<std::uint8_t>(crc & 0xFF));
    CHECK(request[7] == static_cast<std::uint8_t>(crc >> 8));
}
