#include "homeguard/telemetry.hpp"
#include "homeguard/crc32.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <string>
namespace hg {
namespace {
template <typename T, std::size_t N>
void append_numeric_array(std::string& out, const std::array<T, N>& values) {
 out.push_back('[');
 for (std::size_t i = 0; i < N; ++i) {
  if (i != 0) out.push_back(',');
  out += std::to_string(static_cast<int>(values[i]));
 }
 out.push_back(']');
}
template <std::size_t N>
void append_float_array(std::string& out, const std::array<float, N>& values) {
 out.push_back('[');
 for (std::size_t i = 0; i < N; ++i) {
  if (i != 0) out.push_back(',');
  out += std::to_string(values[i]);
 }
 out.push_back(']');
}
template <std::size_t N>
void append_bool_array(std::string& out, const std::array<bool, N>& values) {
 out.push_back('[');
 for (std::size_t i = 0; i < N; ++i) {
  if (i != 0) out.push_back(',');
  out += values[i] ? "true" : "false";
 }
 out.push_back(']');
}
}
std::string telemetry_json(const TelemetryFrame& f) {
 std::string out;
 out.reserve(896);
 out += "{\"sequence\":" + std::to_string(f.sequence);
 out += ",\"uptime_ms\":" + std::to_string(f.uptime_ms);
 out += ",\"rtc_epoch\":" + std::to_string(f.rtc_epoch);
 out += ",\"mode\":" + std::to_string(static_cast<int>(f.mode));
 out += ",\"transport\":\"" + std::string(to_string(f.transport)) + "\"";
 out += ",\"zones\":"; append_numeric_array(out, f.zones);
 out += ",\"pressures\":"; append_numeric_array(out, f.pressures);
 out += ",\"pressure_values\":"; append_float_array(out, f.pressure_values);
 out += ",\"pressure_valid\":"; append_bool_array(out, f.pressure_valid);
 out += ",\"health\":\"" + std::string(to_string(f.health)) + "\"";
 out += ",\"failed_components\":" + std::to_string(f.failed_components);
 out += ",\"temperatures_c\":"; append_float_array(out, f.temperatures_c);
 out += ",\"temperature_valid\":"; append_bool_array(out, f.temperature_valid);
 out += ",\"temperature_count\":" + std::to_string(static_cast<unsigned>(f.temperature_count));
 out += ",\"battery_voltage_v\":" + std::to_string(f.battery_voltage_v);
 out += ",\"battery_current_a\":" + std::to_string(f.battery_current_a);
 out += ",\"battery_power_w\":" + std::to_string(f.battery_power_w);
 out += ",\"battery_valid\":"; out += f.battery_valid ? "true" : "false";
 out += ",\"ac_voltage_v\":" + std::to_string(f.ac_voltage_v);
 out += ",\"ac_current_a\":" + std::to_string(f.ac_current_a);
 out += ",\"ac_power_w\":" + std::to_string(f.ac_power_w);
 out += ",\"ac_energy_kwh\":" + std::to_string(f.ac_energy_kwh);
 out += ",\"ac_frequency_hz\":" + std::to_string(f.ac_frequency_hz);
 out += ",\"ac_power_factor\":" + std::to_string(f.ac_power_factor);
 out += ",\"ac_power_alarm\":"; out += f.ac_power_alarm ? "true" : "false";
 out += ",\"ac_meter_valid\":"; out += f.ac_meter_valid ? "true" : "false";
 out += ",\"crc\":" + std::to_string(f.crc) + "}";
 return out;
}
TelemetryFrame TelemetryBuilder::build(uint64_t up,uint64_t rtc,SystemMode mode,Transport transport,const std::array<ZoneState,5>& zones,const std::array<PressureState,2>& pressures,const HealthMonitor& health,const std::array<float,8>& temperatures_c,const std::array<bool,8>& temperature_valid,uint8_t temperature_count,float battery_voltage_v,float battery_current_a,float battery_power_w,bool battery_valid,const std::array<float,2>& pressure_values,const std::array<bool,2>& pressure_valid,float ac_voltage_v,float ac_current_a,float ac_power_w,float ac_energy_kwh,float ac_frequency_hz,float ac_power_factor,bool ac_power_alarm,bool ac_meter_valid){
 TelemetryFrame f{};f.sequence=++sequence_;f.uptime_ms=up;f.rtc_epoch=rtc;f.mode=mode;f.transport=transport;f.zones=zones;f.pressures=pressures;f.pressure_values=pressure_values;f.pressure_valid=pressure_valid;f.health=health.overall();f.failed_components=health.failed_count();
 f.temperatures_c=temperatures_c;f.temperature_valid=temperature_valid;f.temperature_count=static_cast<uint8_t>(std::min<size_t>(temperature_count,f.temperatures_c.size()));
 f.battery_voltage_v=battery_voltage_v;f.battery_current_a=battery_current_a;f.battery_power_w=battery_power_w;f.battery_valid=battery_valid;
 f.ac_voltage_v=ac_voltage_v;f.ac_current_a=ac_current_a;f.ac_power_w=ac_power_w;f.ac_energy_kwh=ac_energy_kwh;f.ac_frequency_hz=ac_frequency_hz;f.ac_power_factor=ac_power_factor;f.ac_power_alarm=ac_power_alarm;f.ac_meter_valid=ac_meter_valid;
 std::array<std::byte,sizeof(TelemetryFrame)-sizeof(uint32_t)> raw{};std::memcpy(raw.data(),&f,raw.size());f.crc=crc32(raw);return f;
}
}
