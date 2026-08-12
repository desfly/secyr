#pragma once

#include "homeguard/sha256.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace homeguard {

enum class AccessRole : std::uint8_t { Guest, User, Admin };
enum class AuditDecision : std::uint8_t { Allowed, DeniedUnknownUser, DeniedCredential, DeniedRole };

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
    static constexpr std::size_t user_capacity = 8;
    static constexpr std::size_t audit_capacity = 64;

    bool set_user(
        std::string_view id,
        std::string_view name,
        AccessRole role,
        std::string_view pin,
        const std::array<std::uint8_t, 16>& salt,
        bool enabled = true);

    // Persistence path: imports a record that already contains a salted PIN
    // digest. Raw PIN material is never required during restore.
    bool import_user(const AccessUser& user);
    void clear_users();
    [[nodiscard]] const AccessUser* user_at(std::size_t index) const;

    [[nodiscard]] const AccessUser* find_user(std::string_view id) const;
    [[nodiscard]] bool verify_pin(const AccessUser& user, std::string_view pin) const;
    [[nodiscard]] bool role_allows(AccessRole role, std::string_view command) const;
    [[nodiscard]] AuditDecision authenticate(std::string_view actor, std::string_view pin);
    [[nodiscard]] AuditDecision authorize(
        std::string_view actor,
        std::string_view pin,
        std::string_view command);

    [[nodiscard]] std::size_t user_count() const { return user_count_; }
    [[nodiscard]] std::size_t audit_size() const { return audit_size_; }
    [[nodiscard]] const AccessAuditRecord* audit_at_oldest(std::size_t index) const;

private:
    static hg::Sha256Digest derive_pin_digest(
        std::string_view user_id,
        std::string_view pin,
        const std::array<std::uint8_t, 16>& salt);
    static void copy_text(char* destination, std::size_t capacity, std::string_view source);
    void append_audit(std::string_view actor, std::string_view command, AuditDecision decision);

    std::array<AccessUser, user_capacity> users_{};
    std::size_t user_count_{};
    std::array<AccessAuditRecord, audit_capacity> audit_{};
    std::size_t audit_head_{};
    std::size_t audit_size_{};
    std::uint64_t next_audit_sequence_{1};
};

[[nodiscard]] const char* to_string(AccessRole role) noexcept;
[[nodiscard]] const char* to_string(AuditDecision decision) noexcept;

}  // namespace homeguard
