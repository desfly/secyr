#include "homeguard/telemetry_transport.hpp"

#include <algorithm>
#include <sstream>

namespace hg {
namespace {

std::string_view mode_name(SystemMode value) {
    switch (value) {
        case SystemMode::ArmedHome: return "armed_home";
        case SystemMode::ArmedAway: return "armed_away";
        case SystemMode::Alarm: return "alarm";
        case SystemMode::Maintenance: return "maintenance";
        default: return "disarmed";
    }
}

std::string_view zone_name(ZoneState value) {
    switch (value) {
        case ZoneState::Open: return "open";
        case ZoneState::Tamper: return "tamper";
        case ZoneState::Disabled: return "disabled";
        default: return "normal";
    }
}

std::string_view pressure_name(PressureState value) {
    switch (value) {
        case PressureState::Disabled: return "disabled";
        case PressureState::Low: return "low";
        case PressureState::High: return "high";
        case PressureState::SensorFault: return "sensor_fault";
        default: return "normal";
    }
}

}  // namespace

void BearerTokenVerifier::reset(std::string_view token) {
    configured_ = token.size() >= 32U && token.size() <= 256U;
    digest_ = configured_ ? sha256(token) : Sha256Digest{};
}

void BearerTokenVerifier::clear() {
    digest_.fill(0);
    configured_ = false;
}

bool BearerTokenVerifier::authorized(std::string_view header) const {
    constexpr std::string_view prefix = "Bearer ";
    if (!configured_ || !header.starts_with(prefix)) return false;
    const auto token = header.substr(prefix.size());
    if (token.empty() || token.find_first_of("\r\n\t ") != std::string_view::npos) return false;
    return constant_time_equal(digest_, sha256(token));
}

std::string telemetry_json(const TelemetryFrame& frame) {
    std::ostringstream out;
    out << "{\"sequence\":" << frame.sequence
        << ",\"uptimeMs\":" << frame.uptime_ms
        << ",\"rtcEpoch\":" << frame.rtc_epoch
        << ",\"mode\":\"" << mode_name(frame.mode)
        << "\",\"transport\":\"" << to_string(frame.transport)
        << "\",\"health\":\"" << to_string(frame.health)
        << "\",\"failedComponents\":" << frame.failed_components
        << ",\"crc\":" << frame.crc << ",\"zones\":[";
    for (std::size_t index = 0; index < frame.zones.size(); ++index) {
        if (index != 0U) out << ',';
        const auto state = frame.zones[index];
        out << "{\"index\":" << index << ",\"name\":\"Zone " << (index + 1U)
            << "\",\"state\":\"" << zone_name(state)
            << "\",\"enabled\":" << (state == ZoneState::Disabled ? "false" : "true") << '}';
    }
    out << "],\"pressures\":[";
    for (std::size_t index = 0; index < frame.pressures.size(); ++index) {
        if (index != 0U) out << ',';
        out << "{\"index\":" << index
            << ",\"value\":" << frame.pressure_values[index]
            << ",\"unit\":\"mV\",\"valid\":" << (frame.pressure_valid[index] ? "true" : "false")
            << ",\"state\":\"" << pressure_name(frame.pressures[index]) << "\"}";
    }
    out << "],\"temperatures\":[";
    const auto temperature_count = std::min<std::size_t>(frame.temperature_count, frame.temperatures_c.size());
    for (std::size_t index = 0; index < temperature_count; ++index) {
        if (index != 0U) out << ',';
        out << "{\"index\":" << index << ",\"name\":\"Temperature " << (index + 1U)
            << "\",\"celsius\":" << frame.temperatures_c[index]
            << ",\"state\":\"" << (frame.temperature_valid[index] ? "ok" : "sensor_fault") << "\"}";
    }
    out << "],\"powerChannels\":[";
    if (frame.battery_valid) {
        out << "{\"index\":0,\"name\":\"Battery\",\"voltage\":" << frame.battery_voltage_v
            << ",\"current\":" << frame.battery_current_a
            << ",\"power\":" << frame.battery_power_w
            << ",\"state\":\"ok\"}";
    }
    out << "]}";
    return out.str();
}

}  // namespace hg
