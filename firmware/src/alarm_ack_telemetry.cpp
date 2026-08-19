#include "homeguard/alarm_ack_telemetry.hpp"

#include <sstream>

namespace homeguard {

namespace {

std::string escape_json(const std::string& input)
{
    std::string output;
    output.reserve(input.size());
    for (const char value : input) {
        if (value == '"' || value == '\\') {
            output.push_back('\\');
        }
        if (value == '\n') {
            output += "\\n";
        } else if (value != '\r') {
            output.push_back(value);
        }
    }
    return output;
}

}  // namespace

AlarmAckTelemetry alarm_ack_telemetry(
    const AlarmAcknowledgement& state)
{
    return {
        state.alarm_active,
        state.acknowledged,
        state.alarm_sequence,
        state.acknowledged_at_ms,
        state.acknowledged_by,
    };
}

std::string alarm_ack_telemetry_json(
    const AlarmAckTelemetry& frame)
{
    std::ostringstream output;
    output
        << "{"
        << "\"type\":\"alarm_acknowledgement\","
        << "\"alarm_active\":"
        << (frame.alarm_active ? "true" : "false") << ","
        << "\"acknowledged\":"
        << (frame.acknowledged ? "true" : "false") << ","
        << "\"alarm_sequence\":\"" << frame.alarm_sequence << "\","
        << "\"acknowledged_at_ms\":\""
        << frame.acknowledged_at_ms << "\","
        << "\"acknowledged_by\":\""
        << escape_json(frame.acknowledged_by) << "\""
        << "}";
    return output.str();
}

}  // namespace homeguard
