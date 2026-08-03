#include "homeguard/telemetry.hpp"
#include "homeguard/crc32.hpp"
#include <array>
#include <cstring>
namespace hg {
TelemetryFrame TelemetryBuilder::build(uint64_t up,uint64_t rtc,SystemMode mode,Transport transport,const std::array<ZoneState,5>& zones,const std::array<PressureState,2>& pressures,const HealthMonitor& health){TelemetryFrame f{};f.sequence=++sequence_;f.uptime_ms=up;f.rtc_epoch=rtc;f.mode=mode;f.transport=transport;f.zones=zones;f.pressures=pressures;f.health=health.overall();f.failed_components=health.failed_count();std::array<std::byte,sizeof(TelemetryFrame)-sizeof(uint32_t)> raw{};std::memcpy(raw.data(),&f,raw.size());f.crc=crc32(raw);return f;}
}
