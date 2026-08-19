#include "homeguard/alarm_acknowledgement.hpp"
namespace homeguard {
void AlarmAcknowledgementService::on_alarm_state(bool active, std::uint64_t sequence) {
 if (!active) { state_ = {}; return; }
 if (!state_.alarm_active || state_.alarm_sequence != sequence) {
  state_.alarm_active=true; state_.acknowledged=false; state_.alarm_sequence=sequence; state_.acknowledged_at_ms=0; state_.acknowledged_by.clear();
 }
}
AlarmAckResult AlarmAcknowledgementService::acknowledge(std::uint64_t sequence, std::uint64_t received_at_ms, const std::string& actor) {
 if (!state_.alarm_active) return AlarmAckResult::NoActiveAlarm;
 if (sequence==0 || sequence!=state_.alarm_sequence || actor.empty()) return AlarmAckResult::InvalidRequest;
 if (state_.acknowledged) return AlarmAckResult::AlreadyAcknowledged;
 state_.acknowledged=true; state_.acknowledged_at_ms=received_at_ms; state_.acknowledged_by=actor; return AlarmAckResult::Accepted;
}
const AlarmAcknowledgement& AlarmAcknowledgementService::state() const noexcept { return state_; }
}
