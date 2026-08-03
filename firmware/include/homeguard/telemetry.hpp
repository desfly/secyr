#pragma once
#include "homeguard/health_monitor.hpp"
#include "homeguard/pressure.hpp"
#include "homeguard/types.hpp"
#include <array>
#include <cstdint>
namespace hg {
struct TelemetryFrame {
 uint64_t sequence{}; uint64_t uptime_ms{}; uint64_t rtc_epoch{}; SystemMode mode{SystemMode::Disarmed}; Transport transport{Transport::None};
 std::array<ZoneState,5> zones{}; std::array<PressureState,2> pressures{}; HealthState health{HealthState::Unknown}; uint32_t failed_components{}; uint32_t crc{};
};
class TelemetryBuilder {
public: TelemetryFrame build(uint64_t uptime_ms, uint64_t rtc_epoch, SystemMode mode, Transport transport, const std::array<ZoneState,5>& zones, const std::array<PressureState,2>& pressures, const HealthMonitor& health);
private: uint64_t sequence_{};
};
}
