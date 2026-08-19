#pragma once

#include "alarm_ack_command.hpp"

#include <cstdint>
#include <string>

namespace homeguard {

struct AlarmAckApiRequest {
    std::string alarm_sequence;
    std::string actor;
    std::string request_id;
};

struct AlarmAckApiResult {
    int http_status{400};
    std::string body;
};

AlarmAckApiResult handle_alarm_ack_api(
    const AlarmAckApiRequest& request,
    std::uint64_t server_receive_time_ms,
    AlarmAckCommandHandler& handler);

}  // namespace homeguard
