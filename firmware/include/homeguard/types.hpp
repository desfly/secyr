#pragma once
#include <cstdint>
#include <string_view>

namespace hg {
enum class SystemMode : uint8_t { Disarmed, ArmedHome, ArmedAway, Alarm, Maintenance };
enum class Severity : uint8_t { Info, Warning, Alarm, Fault };
enum class ZoneState : uint8_t { Normal, Open, Tamper, Disabled };
enum class PressureState : uint8_t { Disabled, Normal, Low, High, SensorFault };
enum class HealthState : uint8_t { Unknown, Ok, Degraded, Failed };
enum class Transport : uint8_t { None, Ethernet, WifiSta, EmergencyAp };

constexpr std::string_view to_string(Transport v) {
    switch (v) {
        case Transport::Ethernet: return "ethernet";
        case Transport::WifiSta: return "wifi_sta";
        case Transport::EmergencyAp: return "emergency_ap";
        default: return "none";
    }
}

constexpr std::string_view to_string(HealthState v) {
    switch (v) {
        case HealthState::Ok: return "ok";
        case HealthState::Degraded: return "degraded";
        case HealthState::Failed: return "failed";
        default: return "unknown";
    }
}
}
