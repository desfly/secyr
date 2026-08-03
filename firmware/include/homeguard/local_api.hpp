#pragma once

#include "homeguard/challenge.hpp"
#include "homeguard/command.hpp"
#include "homeguard/health_monitor.hpp"
#include "homeguard/sha256.hpp"
#include "homeguard/telemetry.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace hg {

class BearerTokenVerifier {
public:
    BearerTokenVerifier() = default;
    explicit BearerTokenVerifier(std::string_view token) { reset(token); }
    void reset(std::string_view token);
    void clear();
    [[nodiscard]] bool configured() const { return configured_; }
    [[nodiscard]] bool authorized(std::string_view authorization_header) const;
private:
    Sha256Digest digest_{};
    bool configured_{};
};

[[nodiscard]] std::optional<CommandType> parse_command_type(std::string_view value);
[[nodiscard]] std::string_view command_type_name(CommandType value);
[[nodiscard]] std::string_view command_code_name(CommandCode value);
[[nodiscard]] std::string telemetry_json(const TelemetryFrame& frame);
[[nodiscard]] std::string health_json(const HealthMonitor& health, Transport transport);
[[nodiscard]] std::string challenge_json(const Challenge& challenge);
[[nodiscard]] std::string command_result_json(const CommandResult& result);

}  // namespace hg
