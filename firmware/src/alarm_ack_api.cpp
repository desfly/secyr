#include "homeguard/alarm_ack_api.hpp"

#include <charconv>
#include <sstream>

namespace homeguard {

namespace {

bool parse_u64(const std::string& value, std::uint64_t& out)
{
    if (value.empty()) {
        return false;
    }
    const char* first = value.data();
    const char* last = value.data() + value.size();
    const auto result = std::from_chars(first, last, out);
    return result.ec == std::errc{} && result.ptr == last && out != 0;
}

std::string json_escape(const std::string& input)
{
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

}  // namespace

AlarmAckApiResult handle_alarm_ack_api(
    const AlarmAckApiRequest& request,
    std::uint64_t server_receive_time_ms,
    AlarmAckCommandHandler& handler)
{
    std::uint64_t sequence = 0;
    if (!parse_u64(request.alarm_sequence, sequence) ||
        request.actor.empty() ||
        request.request_id.empty()) {
        return {
            400,
            "{\"result\":\"invalid_request\"}"
        };
    }

    const AlarmAckCommand command{
        sequence,
        request.actor,
        request.request_id,
        server_receive_time_ms,
    };

    const AlarmAckResponse response = handler.handle(command);

    int status = 409;
    switch (response.result) {
    case AlarmAckResult::Accepted:
        status = 200;
        break;
    case AlarmAckResult::AlreadyAcknowledged:
        status = 200;
        break;
    case AlarmAckResult::NoActiveAlarm:
        status = 409;
        break;
    case AlarmAckResult::InvalidRequest:
    default:
        status = 400;
        break;
    }

    std::ostringstream json;
    json << "{"
         << "\"result\":\"" << to_string(response.result) << "\","
         << "\"replayed\":" << (response.replayed ? "true" : "false") << ","
         << "\"alarm_sequence\":\"" << response.state.alarm_sequence << "\","
         << "\"acknowledged\":"
         << (response.state.acknowledged ? "true" : "false") << ","
         << "\"acknowledged_at_ms\":\""
         << response.state.acknowledged_at_ms << "\","
         << "\"acknowledged_by\":\""
         << json_escape(response.state.acknowledged_by) << "\""
         << "}";

    return {status, json.str()};
}

}  // namespace homeguard
