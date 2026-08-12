#pragma once

#include "homeguard/sha256.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace homeguard {

enum class AccessRole : std::uint8_t { Guest, User, Admin };
enum class AuditDecision : std::uint8_t {
    Allowed,
    DeniedUnknownUser,
    DeniedCredential,
    DeniedRole,
    DeniedRateLimited,
};

struct AccessUser {
    std::array<char, 24> id{};
    std::array<char, 32> name{};
    AccessRole role{AccessRole::Guest};
    bool enabled{false};
    std::array<std::uint8_t, 16> salt{};
    hg::Sha256Digest pin_digest{};
};

struct AccessAuditRecord {
    std::uint64_t sequence{};
    std::array<char, 24> actor{};
    std::array<char, 32> command{};
    AuditDecision decision{AuditDecision::DeniedUnknownUser};
};

class AccessControl {
public:
    using AuthClock = std::uint64_t (*)();

    static constexpr std::size_t user_capacity = 8;
    static constexpr std::size_t audit_capacity = 64;
    static constexpr std::uint64_t max_auth_backoff_ms = 30000U;

    bool set_user(
        std::string_view id,
        std::string_view name,
        AccessRole role,
        std::string_view pin,
        const std::array<std::uint8_t, 16>& salt,
        bool enabled = true);

    bool import_user(const AccessUser& user);
    void clear_users();
    [[nodiscard]] const AccessUser* user_at(std::size_t index) const;

    [[nodiscard]] const AccessUser* find_user(std::string_view id) const;
    [[nodiscard]] bool verify_pin(const AccessUser& user, std::string_view pin) const;
    [[nodiscard]] bool role_allows(AccessRole role, std::string_view command) const;

    // Firmware installs a monotonic clock once at boot. After that every
    // authenticate()/authorize() caller (HTTP, command router, MQTT, etc.) is
    // transparently protected by the same per-account throttle. Host/offline
    // users that do not install a clock keep deterministic untimed behavior.
    void set_auth_clock(AuthClock clock) noexcept { auth_clock_ = clock; }
    [[nodiscard]] bool auth_throttle_enabled() const noexcept { return auth_clock_ != nullptr; }

    [[nodiscard]] AuditDecision authenticate(std::string_view actor, std::string_view pin);
    [[nodiscard]] AuditDecision authenticate(
        std::string_view actor,
        std::string_view pin,
        std::uint64_t now_ms);
    [[nodiscard]] AuditDecision authorize(
        std::string_view actor,
        std::string_view pin,
        std::string_view command);
    [[nodiscard]] AuditDecision authorize(
        std::string_view actor,
        std::string_view pin,
        std::string_view command,
        std::uint64_t now_ms);
    [[nodiscard]] std::uint64_t authentication_retry_after_ms(
        std::string_view actor,
        std::uint64_t now_ms) const;

    [[nodiscard]] std::size_t user_count() const { return user_count_; }
    [[nodiscard]] std::size_t enabled_admin_count() const;
    [[nodiscard]] bool would_preserve_admin_access(
        std::string_view user_id,
        AccessRole replacement_role,
        bool replacement_enabled) const;
    [[nodiscard]] std::size_t audit_size() const { return audit_size_; }
    [[nodiscard]] const AccessAuditRecord* audit_at_oldest(std::size_t index) const;

private:
    struct AuthThrottleState {
        std::uint8_t failures{};
        std::uint64_t blocked_until_ms{};
    };

    static hg::Sha256Digest derive_pin_digest(
        std::string_view user_id,
        std::string_view pin,
        const std::array<std::uint8_t, 16>& salt);
    static void copy_text(char* destination, std::size_t capacity, std::string_view source);
    static std::uint64_t auth_backoff_ms(std::uint8_t failures);
    [[nodiscard]] std::size_t user_index(std::string_view actor) const;
    [[nodiscard]] AuthThrottleState& throttle_for(std::string_view actor);
    [[nodiscard]] const AuthThrottleState& throttle_for(std::string_view actor) const;
    [[nodiscard]] AuditDecision authenticate_unthrottled(std::string_view actor, std::string_view pin);
    [[nodiscard]] AuditDecision authorize_unthrottled(
        std::string_view actor,
        std::string_view pin,
        std::string_view command);
    void update_throttle(std::string_view actor, AuditDecision decision, std::uint64_t now_ms);
    void append_audit(std::string_view actor, std::string_view command, AuditDecision decision);

    std::array<AccessUser, user_capacity> users_{};
    std::size_t user_count_{};
    std::array<AuthThrottleState, user_capacity> auth_throttles_{};
    AuthThrottleState unknown_auth_throttle_{};
    AuthClock auth_clock_{};
    std::array<AccessAuditRecord, audit_capacity> audit_{};
    std::size_t audit_head_{};
    std::size_t audit_size_{};
    std::uint64_t next_audit_sequence_{1};
};

[[nodiscard]] const char* to_string(AccessRole role) noexcept;
[[nodiscard]] const char* to_string(AuditDecision decision) noexcept;

}  // namespace homeguard
