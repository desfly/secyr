#pragma once

#include <cstdint>

namespace hg {

enum class StartupState : uint8_t {
    Cold,
    SetupAp,
    RestartPending,
    WifiConnecting,
    LocalServicesStarting,
    LocalReady,
    CloudReady,
    Offline,
};

enum class StartupAction : uint8_t {
    None,
    StartSetupAp,
    RestartController,
    StartWifiSta,
    StartLocalServices,
    StartCloud,
    RetryWifi,
};

class StartupFlow {
public:
    StartupAction boot(bool provisioned, bool cloud_configured, uint64_t now_ms);
    StartupAction provisioning_committed();
    StartupAction wifi_connected();
    StartupAction wifi_failed(uint64_t now_ms);
    StartupAction local_services_started();
    StartupAction cloud_connected();
    StartupAction tick(uint64_t now_ms);
    [[nodiscard]] StartupState state() const { return state_; }
    [[nodiscard]] uint32_t retry_count() const { return retry_count_; }
private:
    StartupState state_{StartupState::Cold};
    bool cloud_configured_{};
    uint32_t retry_count_{};
    uint64_t next_retry_at_{};
};

}  // namespace hg
