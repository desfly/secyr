#pragma once
#include "homeguard/health_monitor.hpp"
#include "homeguard/types.hpp"
#include <array>
#include <cstdint>
#include <string>
namespace hg {
struct TelemetryFrame {
 uint64_t sequence{}; uint64_t uptime_ms{}; uint64_t rtc_epoch{}; SystemMode mode{SystemMode::Disarmed}; Transport transport{Transport::None};
 std::array<ZoneState,5> zones{}; std::array<PressureState,2> pressures{}; std::array<float,2> pressure_values{}; std::array<bool,2> pressure_valid{}; HealthState health{HealthState::Unknown}; uint32_t failed_components{};
 std::array<float,8> temperatures_c{}; std::array<bool,8> temperature_valid{}; uint8_t temperature_count{};
 float battery_voltage_v{}; float battery_current_a{}; float battery_power_w{}; bool battery_valid{};
 float ac_voltage_v{}; float ac_current_a{}; float ac_power_w{}; float ac_energy_kwh{}; float ac_frequency_hz{}; float ac_power_factor{}; bool ac_power_alarm{}; bool ac_meter_valid{};
 uint32_t crc{};
};
std::string telemetry_json(const TelemetryFrame& frame);
class TelemetryBuilder {
public:
 TelemetryFrame build(uint64_t uptime_ms, uint64_t rtc_epoch, SystemMode mode, Transport transport, const std::array<ZoneState,5>& zones, const std::array<PressureState,2>& pressures, const HealthMonitor& health, const std::array<float,8>& temperatures_c = {}, const std::array<bool,8>& temperature_valid = {}, uint8_t temperature_count = 0, float battery_voltage_v = 0.0F, float battery_current_a = 0.0F, float battery_power_w = 0.0F, bool battery_valid = false, const std::array<float,2>& pressure_values = {}, const std::array<bool,2>& pressure_valid = {}, float ac_voltage_v = 0.0F, float ac_current_a = 0.0F, float ac_power_w = 0.0F, float ac_energy_kwh = 0.0F, float ac_frequency_hz = 0.0F, float ac_power_factor = 0.0F, bool ac_power_alarm = false, bool ac_meter_valid = false);
private: uint64_t sequence_{};
};
}
