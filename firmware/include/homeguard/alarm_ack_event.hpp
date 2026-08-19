#pragma once

#include "homeguard/alarm_ack_command.hpp"

#include <cstdint>
#include <string>

namespace homeguard {

struct AlarmAckAuditEvent {
    std::uint64_t server_time_ms{0};
    std::string request_id;
    std::string actor;
    std::uint64_t alarm_sequence{0};
    AlarmAckResult result{AlarmAckResult::InvalidRequest};
    bool replayed{false};
};

[[nodiscard]] AlarmAckAuditEvent alarm_ack_audit_event(
    const AlarmAckCommand& command,
    const AlarmAckResponse& response);

[[nodiscard]] std::string alarm_ack_audit_text(
    const AlarmAckAuditEvent& event);

}  // namespace homeguard
