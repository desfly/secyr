#include "homeguard/local_api.hpp"
#include "homeguard/pressure.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>

namespace hg {
namespace {
std::string normalized(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (const char c : value) {
        if (c == '-') out.push_back('_');
        else out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return out;
}
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
}

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

std::optional<CommandType> parse_command_type(std::string_view value) {
    const auto command = normalized(value);
    if (command == "arm_home") return CommandType::ArmHome;
    if (command == "arm_away") return CommandType::ArmAway;
    if (command == "disarm") return CommandType::Disarm;
    if (command == "silence") return CommandType::Silence;
    if (command == "open_valves") return CommandType::OpenValves;
    if (command == "close_valves") return CommandType::CloseValves;
    if (command == "reset_alarm") return CommandType::ResetAlarm;
    if (command == "enter_maintenance") return CommandType::EnterMaintenance;
    if (command == "exit_maintenance") return CommandType::ExitMaintenance;
    return std::nullopt;
}
std::string_view command_type_name(CommandType value) {
    switch (value) {
        case CommandType::ArmHome: return "arm_home";
        case CommandType::ArmAway: return "arm_away";
        case CommandType::Disarm: return "disarm";
        case CommandType::Silence: return "silence";
        case CommandType::OpenValves: return "open_valves";
        case CommandType::CloseValves: return "close_valves";
        case CommandType::ResetAlarm: return "reset_alarm";
        case CommandType::EnterMaintenance: return "enter_maintenance";
        case CommandType::ExitMaintenance: return "exit_maintenance";
    }
    return "invalid";
}
std::string_view command_code_name(CommandCode value) {
    switch (value) {
        case CommandCode::Accepted: return "accepted";
        case CommandCode::Duplicate: return "duplicate";
        case CommandCode::Unauthorized: return "unauthorized";
        case CommandCode::ChallengeRequired: return "challenge_required";
        case CommandCode::ChallengeInvalid: return "challenge_invalid";
        case CommandCode::Unsafe: return "unsafe";
        default: return "invalid";
    }
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
    for (size_t index = 0; index < frame.zones.size(); ++index) {
        if (index != 0U) out << ',';
        const auto state = frame.zones[index];
        out << "{\"index\":" << index << ",\"name\":\"Zone " << (index + 1U)
            << "\",\"state\":\"" << zone_name(state)
            << "\",\"enabled\":" << (state == ZoneState::Disabled ? "false" : "true") << '}';
    }
    out << "],\"pressures\":[";
    for (size_t index = 0; index < frame.pressures.size(); ++index) {
        if (index != 0U) out << ',';
        out << "{\"index\":" << index << ",\"value\":0.0,\"state\":\""
            << pressure_name(frame.pressures[index]) << "\"}";
    }
    out << "],\"temperatures\":[";
    const auto temperature_count = std::min<size_t>(frame.temperature_count, frame.temperatures_c.size());
    for (size_t index = 0; index < temperature_count; ++index) {
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

std::string health_json(const HealthMonitor& health, Transport transport) {
    std::ostringstream out;
    out << "{\"overall\":\"" << to_string(health.overall())
        << "\",\"activeTransport\":\"" << to_string(transport)
        << "\",\"failedCount\":" << health.failed_count()
        << ",\"degradedCount\":" << health.degraded_count() << ",\"components\":[";
    for (size_t index = 0; index < static_cast<size_t>(Component::Count); ++index) {
        if (index != 0U) out << ',';
        const auto component = static_cast<Component>(index);
        const auto entry = health.get(component);
        const auto name = component_name(component);
        out << "{\"id\":\"" << name << "\",\"title\":\"" << name
            << "\",\"state\":\"" << to_string(entry.state)
            << "\",\"changedAtMs\":" << entry.changed_at_ms
            << ",\"failures\":" << entry.consecutive_failures << '}';
    }
    out << "]}";
    return out.str();
}

std::string challenge_json(const Challenge& challenge) {
    std::ostringstream out;
    out << "{\"challenge\":" << challenge.token
        << ",\"command\":\"" << command_type_name(challenge.command)
        << "\",\"expiresAtMs\":" << challenge.expires_at_ms << '}';
    return out.str();
}

std::string command_result_json(const CommandResult& result) {
    std::ostringstream out;
    out << "{\"accepted\":" << (result.executed ? "true" : "false")
        << ",\"duplicate\":" << (result.duplicate ? "true" : "false")
        << ",\"code\":\"" << command_code_name(result.code) << "\"}";
    return out.str();
}

}  // namespace hg
