#pragma once

#include "alarm_acknowledgement.hpp"

#include <cstdint>
#include <string>

namespace homeguard {

struct AlarmAckCommand {
    std::uint64_t alarm_sequence{0};
    std::string actor;
    std::string request_id;
    std::uint64_t server_receive_time_ms{0};
};

struct AlarmAckResponse {
    AlarmAckResult result{AlarmAckResult::InvalidRequest};
    AlarmAcknowledgement state{};
    bool replayed{false};
};

class AlarmAckCommandHandler {
public:
    explicit AlarmAckCommandHandler(AlarmAcknowledgementService& service);

    AlarmAckResponse handle(const AlarmAckCommand& command);

private:
    AlarmAcknowledgementService& service_;
    std::string last_request_id_;
    AlarmAckResponse last_response_{};
};

const char* to_string(AlarmAckResult result) noexcept;

}  // namespace homeguard
