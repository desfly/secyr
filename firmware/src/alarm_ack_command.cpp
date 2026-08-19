#include "homeguard/alarm_ack_command.hpp"

namespace homeguard {

AlarmAckCommandHandler::AlarmAckCommandHandler(
    AlarmAcknowledgementService& service)
    : service_(service)
{
}

AlarmAckResponse AlarmAckCommandHandler::handle(
    const AlarmAckCommand& command)
{
    if (!command.request_id.empty() &&
        command.request_id == last_request_id_) {
        AlarmAckResponse replay = last_response_;
        replay.replayed = true;
        return replay;
    }

    AlarmAckResponse response;
    response.result = service_.acknowledge(
        command.alarm_sequence,
        command.server_receive_time_ms,
        command.actor);
    response.state = service_.state();
    response.replayed = false;

    if (!command.request_id.empty()) {
        last_request_id_ = command.request_id;
        last_response_ = response;
    }

    return response;
}

const char* to_string(AlarmAckResult result) noexcept
{
    switch (result) {
    case AlarmAckResult::Accepted:
        return "accepted";
    case AlarmAckResult::AlreadyAcknowledged:
        return "already_acknowledged";
    case AlarmAckResult::NoActiveAlarm:
        return "no_active_alarm";
    case AlarmAckResult::InvalidRequest:
    default:
        return "invalid_request";
    }
}

}  // namespace homeguard
