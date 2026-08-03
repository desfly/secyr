#include "homeguard/startup_flow.hpp"

#include <algorithm>

namespace hg {

StartupAction StartupFlow::boot(const bool provisioned, const bool cloud_configured, const uint64_t now_ms) {
    cloud_configured_ = cloud_configured;
    retry_count_ = 0;
    next_retry_at_ = now_ms;
    state_ = provisioned ? StartupState::WifiConnecting : StartupState::SetupAp;
    return provisioned ? StartupAction::StartWifiSta : StartupAction::StartSetupAp;
}

StartupAction StartupFlow::provisioning_committed() {
    if (state_ != StartupState::SetupAp) return StartupAction::None;
    state_ = StartupState::RestartPending;
    return StartupAction::RestartController;
}

StartupAction StartupFlow::wifi_connected() {
    if (state_ != StartupState::WifiConnecting && state_ != StartupState::Offline) return StartupAction::None;
    retry_count_ = 0;
    state_ = StartupState::LocalServicesStarting;
    return StartupAction::StartLocalServices;
}

StartupAction StartupFlow::wifi_failed(const uint64_t now_ms) {
    if (state_ != StartupState::WifiConnecting) return StartupAction::None;
    ++retry_count_;
    const uint64_t exponent = std::min<uint32_t>(retry_count_ - 1U, 5U);
    const uint64_t delay_ms = std::min<uint64_t>(1000ULL << exponent, 30000ULL);
    next_retry_at_ = now_ms + delay_ms;
    state_ = StartupState::Offline;
    return StartupAction::None;
}

StartupAction StartupFlow::local_services_started() {
    if (state_ != StartupState::LocalServicesStarting) return StartupAction::None;
    state_ = StartupState::LocalReady;
    return cloud_configured_ ? StartupAction::StartCloud : StartupAction::None;
}

StartupAction StartupFlow::cloud_connected() {
    if (state_ != StartupState::LocalReady) return StartupAction::None;
    state_ = StartupState::CloudReady;
    return StartupAction::None;
}

StartupAction StartupFlow::tick(const uint64_t now_ms) {
    if (state_ != StartupState::Offline || now_ms < next_retry_at_) return StartupAction::None;
    state_ = StartupState::WifiConnecting;
    return StartupAction::RetryWifi;
}

}  // namespace hg
