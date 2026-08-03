#pragma once

#include "homeguard/alarm_acknowledgement.hpp"

#include <cstdint>
#include <string>

namespace homeguard {

struct AlarmAckTelemetry {
    bool alarm_active{false};
    bool acknowledged{false};
    std::uint64_t alarm_sequence{0};
    std::uint64_t acknowledged_at_ms{0};
    std::string acknowledged_by;
};

[[nodiscard]] AlarmAckTelemetry alarm_ack_telemetry(
    const AlarmAcknowledgement& state);

[[nodiscard]] std::string alarm_ack_telemetry_json(
    const AlarmAckTelemetry& frame);

}  // namespace homeguard
