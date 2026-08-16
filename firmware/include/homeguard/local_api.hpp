#pragma once

#include "homeguard/challenge.hpp"
#include "homeguard/command.hpp"
#include "homeguard/health_monitor.hpp"
#include "homeguard/telemetry_transport.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace hg {

[[nodiscard]] std::optional<CommandType> parse_command_type(std::string_view value);
[[nodiscard]] std::string_view command_type_name(CommandType value);
[[nodiscard]] std::string_view command_code_name(CommandCode value);
[[nodiscard]] std::string health_json(const HealthMonitor& health, Transport transport);
[[nodiscard]] std::string challenge_json(const Challenge& challenge);
[[nodiscard]] std::string command_result_json(const CommandResult& result);

}  // namespace hg
