#include "homeguard/device_api_model.hpp"

#include <sstream>

namespace homeguard {

namespace {

std::string escape_json(const std::string& input)
{
    std::string output;
    output.reserve(input.size());
    for (const char value : input) {
        switch (value) {
        case '"':
        case '\\':
            output.push_back('\\');
            output.push_back(value);
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            break;
        default:
            output.push_back(value);
            break;
        }
    }
    return output;
}

}  // namespace

const char* to_string(SecurityMode mode) noexcept
{
    switch (mode) {
    case SecurityMode::Disarmed:
        return "disarmed";
    case SecurityMode::ArmedHome:
        return "armed_home";
    case SecurityMode::ArmedAway:
        return "armed_away";
    case SecurityMode::Alarm:
        return "alarm";
    default:
        return "disarmed";
    }
}

const char* to_string(CommandResultCode code) noexcept
{
    switch (code) {
    case CommandResultCode::Accepted:
        return "accepted";
    case CommandResultCode::Duplicate:
        return "duplicate";
    case CommandResultCode::Rejected:
        return "rejected";
    case CommandResultCode::Unauthorized:
        return "unauthorized";
    case CommandResultCode::Invalid:
        return "invalid";
    case CommandResultCode::HardwareFault:
        return "hardware_fault";
    default:
        return "invalid";
    }
}

std::string device_state_json(const DeviceApiState& state)
{
    std::ostringstream out;
    out << "{"
        << "\"sequence\":\"" << state.sequence << "\","
        << "\"server_time_ms\":\"" << state.server_time_ms << "\","
        << "\"security_mode\":\"" << to_string(state.security_mode) << "\","
        << "\"corridor_light\":" << (state.corridor_light ? "true" : "false") << ","
        << "\"siren\":" << (state.siren ? "true" : "false") << ","
        << "\"mains_present\":" << (state.mains_present ? "true" : "false") << ","
        << "\"battery_voltage_v\":" << state.battery_voltage_v << ","
        << "\"battery_current_a\":" << state.battery_current_a << ","
        << "\"cold_pressure_bar\":" << state.cold_pressure_bar << ","
        << "\"hot_pressure_bar\":" << state.hot_pressure_bar << ","
        << "\"cold_temperature_c\":" << state.cold_temperature_c << ","
        << "\"hot_temperature_c\":" << state.hot_temperature_c << ","
        << "\"zones\":[";

    for (std::size_t index = 0; index < state.zones.size(); ++index) {
        const auto& zone = state.zones[index];
        if (index != 0) {
            out << ",";
        }
        out << "{"
            << "\"id\":\"" << escape_json(zone.id) << "\","
            << "\"title\":\"" << escape_json(zone.title) << "\","
            << "\"state\":\"" << escape_json(zone.state) << "\","
            << "\"always_on\":" << (zone.always_on ? "true" : "false") << ","
            << "\"alarm\":" << (zone.alarm ? "true" : "false") << ","
            << "\"millivolts\":" << zone.millivolts
            << "}";
    }

    out << "],\"valves\":[";
    for (std::size_t index = 0; index < state.valves.size(); ++index) {
        const auto& valve = state.valves[index];
        if (index != 0) {
            out << ",";
        }
        out << "{"
            << "\"id\":\"" << escape_json(valve.id) << "\","
            << "\"state\":\"" << escape_json(valve.state) << "\","
            << "\"emergency_latched\":"
            << (valve.emergency_latched ? "true" : "false") << ","
            << "\"fault_count\":" << valve.fault_count
            << "}";
    }

    out << "]}";
    return out.str();
}

std::string command_response_json(
    const DeviceCommandResponse& response)
{
    std::ostringstream out;
    out << "{"
        << "\"code\":\"" << to_string(response.code) << "\","
        << "\"message\":\"" << escape_json(response.message) << "\","
        << "\"state_sequence\":\"" << response.state_sequence << "\""
        << "}";
    return out.str();
}

}  // namespace homeguard
