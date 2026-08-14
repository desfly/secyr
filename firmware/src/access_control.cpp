#include "homeguard/access_control.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace homeguard {
namespace {
std::string salt_hex(const std::array<std::uint8_t, 16>& salt) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(salt.size() * 2U);
    for (const auto byte : salt) {
        out.push_back(digits[(byte >> 4U) & 0x0fU]);
        out.push_back(digits[byte & 0x0fU]);
    }
    return out;
}

void append_digest_hex(std::string& out, const hg::Sha256Digest& digest) {
    static constexpr char digits[] = "0123456789abcdef";
    for (const auto byte : digest) {
        out.push_back(digits[(byte >> 4U) & 0x0fU]);
        out.push_back(digits[byte & 0x0fU]);
    }
}

bool terminated(const auto& text) {
    return std::find(text.begin(), text.end(), '\0') != text.end();
}
}

void AccessControl::copy_text(char* destination, std::size_t capacity, std::string_view source) {
    if (capacity == 0U) return;
    const auto count = std::min(source.size(), capacity - 1U);
    std::copy_n(source.data(), count, destination);
    destination[count] = '\0';
}

hg::Sha256Digest AccessControl::derive_pin_digest(
    std::string_view user_id,
    std::string_view pin,
    const std::array<std::uint8_t, 16>& salt) {
    const auto salt_text = salt_hex(salt);
    std::string material;
    material.reserve(128U);
    material.append("HomeGuard-S3|PIN|");
    material.append(user_id);
    material.push_back('|');
    material.append(salt_text);
    material.push_back('|');
    material.append(pin);

    auto digest = hg::sha256(material);
    for (std::uint32_t round = 1; round < 4096U; ++round) {
        material.clear();
        append_digest_hex(material, digest);
        material.append(user_id);
        material.append(salt_text);
        digest = hg::sha256(material);
    }
    return digest;
}

bool AccessControl::set_user(
    std::string_view id,
    std::string_view name,
    AccessRole role,
    std::string_view pin,
    const std::array<std::uint8_t, 16>& salt,
    bool enabled) {
    if (id.empty() || pin.size() < 4U || pin.size() > 12U) return false;

    AccessUser record{};
    copy_text(record.id.data(), record.id.size(), id);
    copy_text(record.name.data(), record.name.size(), name);
    record.role = role;
    record.enabled = enabled;
    record.salt = salt;
    record.pin_digest = derive_pin_digest(id, pin, salt);
    return import_user(record);
}

std::size_t AccessControl::user_index(std::string_view actor) const {
    for (std::size_t i = 0; i < user_count_; ++i) {
        if (actor == users_[i].id.data()) return i;
    }
    return user_capacity;
}

bool AccessControl::import_user(const AccessUser& user) {
    if (!terminated(user.id) || !terminated(user.name) || user.id[0] == '\0') return false;
    if (static_cast<std::uint8_t>(user.role) > static_cast<std::uint8_t>(AccessRole::Admin)) return false;

    const std::string_view id{user.id.data()};
    auto index = user_index(id);
    if (index == user_capacity) {
        if (user_count_ >= user_capacity) return false;
        index = user_count_++;
    }
    users_[index] = user;
    auth_throttles_[index] = {};
    return true;
}

void AccessControl::clear_users() {
    for (auto& user : users_) user = AccessUser{};
    for (auto& throttle : auth_throttles_) throttle = AuthThrottleState{};
    unknown_auth_throttle_ = {};
    user_count_ = 0;
}

const AccessUser* AccessControl::user_at(std::size_t index) const {
    return index < user_count_ ? &users_[index] : nullptr;
}

const AccessUser* AccessControl::find_user(std::string_view id) const {
    const auto index = user_index(id);
    return index < user_count_ ? &users_[index] : nullptr;
}

std::size_t AccessControl::enabled_admin_count() const {
    std::size_t count = 0;
    for (std::size_t i = 0; i < user_count_; ++i) {
        if (users_[i].enabled && users_[i].role == AccessRole::Admin) ++count;
    }
    return count;
}

bool AccessControl::would_preserve_admin_access(
    std::string_view user_id,
    AccessRole replacement_role,
    bool replacement_enabled) const {
    auto projected = enabled_admin_count();
    const auto* existing = find_user(user_id);
    const bool existing_enabled_admin =
        existing != nullptr && existing->enabled && existing->role == AccessRole::Admin;
    const bool replacement_enabled_admin =
        replacement_enabled && replacement_role == AccessRole::Admin;

    if (existing_enabled_admin && !replacement_enabled_admin) {
        if (projected == 0U) return false;
        --projected;
    } else if (!existing_enabled_admin && replacement_enabled_admin) {
        ++projected;
    }
    return projected > 0U;
}

bool AccessControl::verify_pin(const AccessUser& user, std::string_view pin) const {
    if (!user.enabled || pin.empty()) return false;
    return hg::constant_time_equal(user.pin_digest, derive_pin_digest(user.id.data(), pin, user.salt));
}

bool AccessControl::role_allows(AccessRole role, std::string_view command) const {
    if (role == AccessRole::Admin) return true;
    if (role == AccessRole::Guest) return false;

    return command == "security.arm_home" || command == "arm_home" ||
           command == "security.arm_away" || command == "arm_away" ||
           command == "security.disarm" || command == "disarm" ||
           command == "valve.close" || command == "close_valves" ||
           command == "valve.open" || command == "open_valves";
}

std::uint64_t AccessControl::auth_backoff_ms(std::uint8_t failures) {
    switch (failures) {
        case 0: return 0U;
        case 1: return 250U;
        case 2: return 500U;
        case 3: return 1000U;
        case 4: return 2000U;
        case 5: return 5000U;
        case 6: return 10000U;
        default: return max_auth_backoff_ms;
    }
}

AccessControl::AuthThrottleState& AccessControl::throttle_for(std::string_view actor) {
    const auto index = user_index(actor);
    return index < user_count_ ? auth_throttles_[index] : unknown_auth_throttle_;
}

const AccessControl::AuthThrottleState& AccessControl::throttle_for(std::string_view actor) const {
    const auto index = user_index(actor);
    return index < user_count_ ? auth_throttles_[index] : unknown_auth_throttle_;
}

std::uint64_t AccessControl::authentication_retry_after_ms(
    std::string_view actor,
    std::uint64_t now_ms) const {
    const auto& state = throttle_for(actor);
    return state.blocked_until_ms > now_ms ? state.blocked_until_ms - now_ms : 0U;
}

void AccessControl::update_throttle(
    std::string_view actor,
    AuditDecision decision,
    std::uint64_t now_ms) {
    auto& state = throttle_for(actor);

    if (decision == AuditDecision::Allowed || decision == AuditDecision::DeniedRole) {
        state = {};
        return;
    }

    if (decision != AuditDecision::DeniedCredential &&
        decision != AuditDecision::DeniedUnknownUser) {
        return;
    }

    if (state.failures < 0xffU) ++state.failures;
    state.blocked_until_ms = now_ms + auth_backoff_ms(state.failures);
}

void AccessControl::append_audit(
    std::string_view actor,
    std::string_view command,
    AuditDecision decision) {
    AccessAuditRecord record{};
    record.sequence = next_audit_sequence_++;
    copy_text(record.actor.data(), record.actor.size(), actor);
    copy_text(record.command.data(), record.command.size(), command);
    record.decision = decision;
    audit_[audit_head_] = record;
    audit_head_ = (audit_head_ + 1U) % audit_capacity;
    if (audit_size_ < audit_capacity) ++audit_size_;
}

AuditDecision AccessControl::authenticate_unthrottled(
    std::string_view actor,
    std::string_view pin) {
    const auto* user = find_user(actor);
    if (user == nullptr || !user->enabled) {
        append_audit(actor, "access.login", AuditDecision::DeniedUnknownUser);
        return AuditDecision::DeniedUnknownUser;
    }
    if (!verify_pin(*user, pin)) {
        append_audit(actor, "access.login", AuditDecision::DeniedCredential);
        return AuditDecision::DeniedCredential;
    }
    append_audit(actor, "access.login", AuditDecision::Allowed);
    return AuditDecision::Allowed;
}

AuditDecision AccessControl::authenticate(std::string_view actor, std::string_view pin) {
    if (auth_clock_ != nullptr) return authenticate(actor, pin, auth_clock_());
    return authenticate_unthrottled(actor, pin);
}

AuditDecision AccessControl::authenticate(
    std::string_view actor,
    std::string_view pin,
    std::uint64_t now_ms) {
    if (authentication_retry_after_ms(actor, now_ms) != 0U) {
        append_audit(actor, "access.login", AuditDecision::DeniedRateLimited);
        return AuditDecision::DeniedRateLimited;
    }
    const auto decision = authenticate_unthrottled(actor, pin);
    update_throttle(actor, decision, now_ms);
    return decision;
}

AuditDecision AccessControl::authorize_unthrottled(
    std::string_view actor,
    std::string_view pin,
    std::string_view command) {
    const auto* user = find_user(actor);
    if (user == nullptr || !user->enabled) {
        append_audit(actor, command, AuditDecision::DeniedUnknownUser);
        return AuditDecision::DeniedUnknownUser;
    }
    if (!verify_pin(*user, pin)) {
        append_audit(actor, command, AuditDecision::DeniedCredential);
        return AuditDecision::DeniedCredential;
    }
    if (!role_allows(user->role, command)) {
        append_audit(actor, command, AuditDecision::DeniedRole);
        return AuditDecision::DeniedRole;
    }
    append_audit(actor, command, AuditDecision::Allowed);
    return AuditDecision::Allowed;
}

AuditDecision AccessControl::authorize(
    std::string_view actor,
    std::string_view pin,
    std::string_view command) {
    if (auth_clock_ != nullptr) return authorize(actor, pin, command, auth_clock_());
    return authorize_unthrottled(actor, pin, command);
}

AuditDecision AccessControl::authorize(
    std::string_view actor,
    std::string_view pin,
    std::string_view command,
    std::uint64_t now_ms) {
    if (authentication_retry_after_ms(actor, now_ms) != 0U) {
        append_audit(actor, command, AuditDecision::DeniedRateLimited);
        return AuditDecision::DeniedRateLimited;
    }
    const auto decision = authorize_unthrottled(actor, pin, command);
    update_throttle(actor, decision, now_ms);
    return decision;
}

const AccessAuditRecord* AccessControl::audit_at_oldest(std::size_t index) const {
    if (index >= audit_size_) return nullptr;
    const auto oldest = (audit_head_ + audit_capacity - audit_size_) % audit_capacity;
    return &audit_[(oldest + index) % audit_capacity];
}

const char* to_string(AccessRole role) noexcept {
    switch (role) {
        case AccessRole::Admin: return "admin";
        case AccessRole::User: return "user";
        default: return "guest";
    }
}

const char* to_string(AuditDecision decision) noexcept {
    switch (decision) {
        case AuditDecision::Allowed: return "allowed";
        case AuditDecision::DeniedCredential: return "denied_credential";
        case AuditDecision::DeniedRole: return "denied_role";
        case AuditDecision::DeniedRateLimited: return "denied_rate_limited";
        default: return "denied_unknown_user";
    }
}

}  // namespace homeguard
