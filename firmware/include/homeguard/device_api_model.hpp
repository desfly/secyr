#pragma once

#include <array>
#include <cstdint>
#include <string>

namespace homeguard {

enum class SecurityMode {
    Disarmed,
    ArmedHome,
    ArmedAway,
    Alarm,
};

enum class CommandResultCode {
    Accepted,
    Duplicate,
    Rejected,
    Unauthorized,
    Invalid,
    HardwareFault,
};

struct ZoneApiState {
    std::string id;
    std::string title;
    std::string state;
    bool always_on{false};
    bool alarm{false};
    float millivolts{0.0F};
};

struct ValveApiState {
    std::string id;
    std::string state;
    bool emergency_latched{false};
    std::uint32_t fault_count{0};
};

struct DeviceApiState {
    std::uint64_t sequence{0};
    std::uint64_t server_time_ms{0};
    SecurityMode security_mode{SecurityMode::Disarmed};
    bool corridor_light{false};
    bool siren{false};
    bool mains_present{true};
    float battery_voltage_v{0.0F};
    float battery_current_a{0.0F};
    float cold_pressure_bar{0.0F};
    float hot_pressure_bar{0.0F};
    float cold_temperature_c{0.0F};
    float hot_temperature_c{0.0F};
    std::array<ZoneApiState, 5> zones{};
    std::array<ValveApiState, 2> valves{};
};

struct DeviceCommandRequest {
    std::string request_id;
    std::string actor;
    std::string command;
    std::string target;
    std::string value;
};

struct DeviceCommandResponse {
    CommandResultCode code{CommandResultCode::Invalid};
    std::string message;
    std::uint64_t state_sequence{0};
};

const char* to_string(SecurityMode mode) noexcept;
const char* to_string(CommandResultCode code) noexcept;

std::string device_state_json(const DeviceApiState& state);
std::string command_response_json(const DeviceCommandResponse& response);

}  // namespace homeguard
