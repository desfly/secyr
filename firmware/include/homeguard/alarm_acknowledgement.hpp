#pragma once
#include <cstdint>
#include <string>
namespace homeguard {
enum class AlarmAckResult { Accepted, AlreadyAcknowledged, NoActiveAlarm, InvalidRequest };
struct AlarmAcknowledgement { bool alarm_active{false}; bool acknowledged{false}; std::uint64_t alarm_sequence{0}; std::uint64_t acknowledged_at_ms{0}; std::string acknowledged_by; };
class AlarmAcknowledgementService {
public:
 void on_alarm_state(bool active, std::uint64_t sequence);
 AlarmAckResult acknowledge(std::uint64_t sequence, std::uint64_t received_at_ms, const std::string& actor);
 const AlarmAcknowledgement& state() const noexcept;
private: AlarmAcknowledgement state_{};
};
}
