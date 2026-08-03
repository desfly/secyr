#include "homeguard/cloud_link.hpp"
#include <algorithm>
#include <utility>

namespace hg {
void CloudLink::configure(CloudLinkConfig config) {
    config_ = std::move(config);
    backoff_ms_ = std::max<uint32_t>(1, config_.base_backoff_ms);
    next_retry_at_ = 0;
    if (!config_.enabled) state_ = CloudLinkState::Disabled;
    else state_ = valid_config() ? CloudLinkState::WaitingForNetwork : CloudLinkState::Fault;
}

bool CloudLink::valid_config() const {
    return !config_.endpoint.empty() && !config_.device_id.empty() && config_.port != 0 &&
           config_.base_backoff_ms > 0 && config_.max_backoff_ms >= config_.base_backoff_ms;
}

CloudAction CloudLink::tick(bool network_available, uint64_t now_ms) {
    if (!config_.enabled) {
        const bool was_active = state_ == CloudLinkState::Connecting || state_ == CloudLinkState::Online;
        state_ = CloudLinkState::Disabled;
        return was_active ? CloudAction::Disconnect : CloudAction::None;
    }
    if (!valid_config()) { state_ = CloudLinkState::Fault; return CloudAction::None; }
    if (!network_available) {
        const bool was_active = state_ == CloudLinkState::Connecting || state_ == CloudLinkState::Online;
        state_ = CloudLinkState::WaitingForNetwork;
        return was_active ? CloudAction::Disconnect : CloudAction::None;
    }
    if (state_ == CloudLinkState::WaitingForNetwork || state_ == CloudLinkState::ReadyToConnect) {
        state_ = CloudLinkState::ReadyToConnect;
        return CloudAction::Connect;
    }
    if (state_ == CloudLinkState::Backoff && now_ms >= next_retry_at_) {
        state_ = CloudLinkState::ReadyToConnect;
        return CloudAction::Connect;
    }
    return CloudAction::None;
}

void CloudLink::on_connecting(uint64_t) { if (state_ != CloudLinkState::Fault) state_ = CloudLinkState::Connecting; }
void CloudLink::on_connected(uint64_t) {
    state_ = CloudLinkState::Online;
    backoff_ms_ = std::max<uint32_t>(1, config_.base_backoff_ms);
    next_retry_at_ = 0;
}
void CloudLink::schedule_retry(uint64_t now_ms) {
    state_ = CloudLinkState::Backoff;
    next_retry_at_ = now_ms + backoff_ms_;
    backoff_ms_ = std::min(config_.max_backoff_ms, backoff_ms_ > config_.max_backoff_ms / 2 ? config_.max_backoff_ms : backoff_ms_ * 2);
}
void CloudLink::on_disconnected(uint64_t now_ms, bool authentication_failure) {
    if (authentication_failure) { state_ = CloudLinkState::Fault; return; }
    if (!config_.enabled) { state_ = CloudLinkState::Disabled; return; }
    schedule_retry(now_ms);
}
}
