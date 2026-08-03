#pragma once
#include "homeguard/sha256.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace hg {
enum class ProvisioningState : uint8_t { Factory, SetupAp, Authorized, Applying, Provisioned, Locked, Fault };
enum class ProvisioningCode : uint8_t { Accepted, InvalidState, Expired, InvalidProof, LockedOut, InvalidPayload, StorageFailure, AlreadyProvisioned };

struct ProvisioningPolicy {
    uint32_t code_ttl_ms{600000};
    uint32_t session_ttl_ms{900000};
    uint8_t max_attempts{5};
    bool require_cloud_claim{false};
};

struct ProvisioningPayload {
    std::string wifi_ssid;
    std::string wifi_password;
    std::string cloud_endpoint;
    std::string cloud_token;
    std::string local_api_token;
    std::string owner_label;
    [[nodiscard]] bool valid(const ProvisioningPolicy& policy) const;
    void clear_secrets();
};

struct ProvisioningStatus {
    ProvisioningState state{ProvisioningState::Factory};
    uint8_t failed_attempts{};
    uint64_t expires_at_ms{};
    bool authorized{};
    bool has_pending_payload{};
};

class ProvisioningSession {
public:
    explicit ProvisioningSession(ProvisioningPolicy policy = {});
    ProvisioningCode begin(std::string_view pairing_code, std::string_view certificate_sha256, uint64_t now_ms);
    ProvisioningCode authorize(std::string_view pairing_code, std::string_view certificate_sha256, uint64_t now_ms);
    ProvisioningCode submit(ProvisioningPayload payload, uint64_t now_ms);
    ProvisioningCode commit(bool storage_ok, uint64_t now_ms);
    bool expire_if_needed(uint64_t now_ms);
    void abort();
    bool factory_reset(bool physical_presence);
    [[nodiscard]] ProvisioningStatus status(uint64_t now_ms) const;
    [[nodiscard]] const std::optional<ProvisioningPayload>& pending() const { return pending_; }
private:
    [[nodiscard]] bool expired(uint64_t now_ms) const;
    void lock();
    ProvisioningPolicy policy_{};
    ProvisioningState state_{ProvisioningState::Factory};
    Sha256Digest code_digest_{};
    Sha256Digest certificate_digest_{};
    uint64_t started_at_ms_{};
    uint64_t expires_at_ms_{};
    uint8_t failed_attempts_{};
    std::optional<ProvisioningPayload> pending_{};
};

class ProvisioningShutdownGate {
public:
    void arm(uint64_t now_ms, uint32_t delay_ms);
    void clear();
    [[nodiscard]] bool armed() const { return due_at_ms_ != 0U; }
    [[nodiscard]] bool due(uint64_t now_ms) const;
private:
    uint64_t due_at_ms_{};
};

[[nodiscard]] bool valid_pairing_code(std::string_view code);
[[nodiscard]] bool valid_sha256_hex(std::string_view fingerprint);
[[nodiscard]] std::string format_pairing_code(uint32_t random_value);
}
