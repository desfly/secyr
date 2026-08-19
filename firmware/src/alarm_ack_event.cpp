#include "homeguard/alarm_ack_event.hpp"

#include <sstream>

namespace homeguard {

AlarmAckAuditEvent alarm_ack_audit_event(
    const AlarmAckCommand& command,
    const AlarmAckResponse& response)
{
    return {
        command.server_receive_time_ms,
        command.request_id,
        command.actor,
        command.alarm_sequence,
        response.result,
        response.replayed,
    };
}

std::string alarm_ack_audit_text(
    const AlarmAckAuditEvent& event)
{
    std::ostringstream output;
    output
        << "alarm_ack"
        << " request_id=" << event.request_id
        << " actor=" << event.actor
        << " alarm_sequence=" << event.alarm_sequence
        << " result=" << to_string(event.result)
        << " replayed=" << (event.replayed ? "true" : "false")
        << " server_time_ms=" << event.server_time_ms;
    return output.str();
}

}  // namespace homeguard
