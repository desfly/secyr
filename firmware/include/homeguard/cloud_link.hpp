#pragma once
#include <cstdint>
#include <string>

namespace hg {
enum class CloudLinkState : uint8_t { Disabled, WaitingForNetwork, ReadyToConnect, Connecting, Online, Backoff, Fault };
enum class CloudAction : uint8_t { None, Connect, Disconnect };

struct CloudLinkConfig {
    bool enabled{false};
    std::string endpoint;
    std::string device_id;
    uint16_t port{8883};
    uint32_t base_backoff_ms{1000};
    uint32_t max_backoff_ms{60000};
};

class CloudLink {
public:
    void configure(CloudLinkConfig config);
    [[nodiscard]] CloudAction tick(bool network_available, uint64_t now_ms);
    void on_connecting(uint64_t now_ms);
    void on_connected(uint64_t now_ms);
    void on_disconnected(uint64_t now_ms, bool authentication_failure = false);
    [[nodiscard]] CloudLinkState state() const { return state_; }
    [[nodiscard]] bool can_publish() const { return state_ == CloudLinkState::Online; }
    [[nodiscard]] uint64_t next_retry_at() const { return next_retry_at_; }
    [[nodiscard]] uint32_t current_backoff_ms() const { return backoff_ms_; }
    [[nodiscard]] const CloudLinkConfig& config() const { return config_; }
private:
    [[nodiscard]] bool valid_config() const;
    void schedule_retry(uint64_t now_ms);
    CloudLinkConfig config_{};
    CloudLinkState state_{CloudLinkState::Disabled};
    uint32_t backoff_ms_{1000};
    uint64_t next_retry_at_{};
};
}
