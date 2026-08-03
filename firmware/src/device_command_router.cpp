#include "homeguard/device_command_router.hpp"

namespace homeguard {

DeviceCommandRouter::DeviceCommandRouter(DeviceApiState& state)
    : state_(state)
{
}

bool DeviceCommandRouter::is_duplicate(
    const std::string& request_id) const
{
    for (const auto& remembered : recent_request_ids_) {
        if (remembered == request_id) {
            return true;
        }
    }
    return false;
}

void DeviceCommandRouter::remember(
    const std::string& request_id)
{
    recent_request_ids_.push_back(request_id);
    while (recent_request_ids_.size() > 64) {
        recent_request_ids_.pop_front();
    }
}

DeviceCommandResponse DeviceCommandRouter::handle(
    const DeviceCommandRequest& request)
{
    if (request.request_id.empty() ||
        request.actor.empty() ||
        request.command.empty()) {
        return {
            CommandResultCode::Invalid,
            "required field is empty",
            state_.sequence,
        };
    }

    if (is_duplicate(request.request_id)) {
        return {
            CommandResultCode::Duplicate,
            "request was already processed",
            state_.sequence,
        };
    }

    DeviceCommandResponse response{
        CommandResultCode::Rejected,
        "unsupported command",
        state_.sequence,
    };

    if (request.command == "security.arm_home") {
        state_.security_mode = SecurityMode::ArmedHome;
        response = {
            CommandResultCode::Accepted,
            "armed home",
            ++state_.sequence,
        };
    } else if (request.command == "security.arm_away") {
        state_.security_mode = SecurityMode::ArmedAway;
        response = {
            CommandResultCode::Accepted,
            "armed away",
            ++state_.sequence,
        };
    } else if (request.command == "security.disarm") {
        state_.security_mode = SecurityMode::Disarmed;
        state_.siren = false;
        response = {
            CommandResultCode::Accepted,
            "disarmed",
            ++state_.sequence,
        };
    } else if (request.command == "light.set") {
        if (request.value != "on" && request.value != "off") {
            response = {
                CommandResultCode::Invalid,
                "light value must be on or off",
                state_.sequence,
            };
        } else {
            state_.corridor_light = request.value == "on";
            response = {
                CommandResultCode::Accepted,
                state_.corridor_light ? "light on" : "light off",
                ++state_.sequence,
            };
        }
    } else if (request.command == "valve.close") {
        const std::size_t index =
            request.target == "hot" ? 1U : 0U;
        state_.valves[index].state = "closing";
        state_.valves[index].emergency_latched = true;
        response = {
            CommandResultCode::Accepted,
            "valve closing",
            ++state_.sequence,
        };
    } else if (request.command == "valve.open") {
        const std::size_t index =
            request.target == "hot" ? 1U : 0U;

        if (state_.valves[index].emergency_latched) {
            response = {
                CommandResultCode::Rejected,
                "emergency latch blocks opening",
                state_.sequence,
            };
        } else {
            state_.valves[index].state = "opening";
            response = {
                CommandResultCode::Accepted,
                "valve opening",
                ++state_.sequence,
            };
        }
    } else if (request.command == "valve.clear_latch") {
        const std::size_t index =
            request.target == "hot" ? 1U : 0U;
        state_.valves[index].emergency_latched = false;
        response = {
            CommandResultCode::Accepted,
            "emergency latch cleared",
            ++state_.sequence,
        };
    }

    if (response.code == CommandResultCode::Accepted ||
        response.code == CommandResultCode::Rejected ||
        response.code == CommandResultCode::Invalid) {
        remember(request.request_id);
    }

    return response;
}

}  // namespace homeguard
