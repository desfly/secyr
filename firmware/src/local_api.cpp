#include "homeguard/local_api.hpp"

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

}  // namespace

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

std::string health_json(const HealthMonitor& health, Transport transport) {
    std::ostringstream out;
    out << "{\"overall\":\"" << to_string(health.overall())
        << "\",\"activeTransport\":\"" << to_string(transport)
        << "\",\"failedCount\":" << health.failed_count()
        << ",\"degradedCount\":" << health.degraded_count() << ",\"components\":[";
    for (std::size_t index = 0; index < static_cast<std::size_t>(Component::Count); ++index) {
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
