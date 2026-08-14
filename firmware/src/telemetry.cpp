#include "homeguard/telemetry.hpp"
#include "homeguard/crc32.hpp"
#include <algorithm>
#include <array>
#include <cstring>
namespace hg {
TelemetryFrame TelemetryBuilder::build(uint64_t up,uint64_t rtc,SystemMode mode,Transport transport,const std::array<ZoneState,5>& zones,const std::array<PressureState,2>& pressures,const HealthMonitor& health,const std::array<float,8>& temperatures_c,const std::array<bool,8>& temperature_valid,uint8_t temperature_count,float battery_voltage_v,float battery_current_a,float battery_power_w,bool battery_valid,const std::array<float,2>& pressure_values,const std::array<bool,2>& pressure_valid){
 TelemetryFrame f{};f.sequence=++sequence_;f.uptime_ms=up;f.rtc_epoch=rtc;f.mode=mode;f.transport=transport;f.zones=zones;f.pressures=pressures;f.pressure_values=pressure_values;f.pressure_valid=pressure_valid;f.health=health.overall();f.failed_components=health.failed_count();
 f.temperatures_c=temperatures_c;f.temperature_valid=temperature_valid;f.temperature_count=static_cast<uint8_t>(std::min<size_t>(temperature_count,f.temperatures_c.size()));
 f.battery_voltage_v=battery_voltage_v;f.battery_current_a=battery_current_a;f.battery_power_w=battery_power_w;f.battery_valid=battery_valid;
 std::array<std::byte,sizeof(TelemetryFrame)-sizeof(uint32_t)> raw{};std::memcpy(raw.data(),&f,raw.size());f.crc=crc32(raw);return f;
}
}
