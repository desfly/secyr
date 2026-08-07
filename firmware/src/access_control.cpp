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
    std::string material{"HomeGuard-S3|PIN|"};
    material.append(user_id);
    material.push_back('|');
    material.append(salt_hex(salt));
    material.push_back('|');
    material.append(pin);

    auto digest = hg::sha256(material);
    // PINs have low entropy. Iteration makes offline guessing materially more
    // expensive while keeping the implementation dependency-free on ESP32-S3.
    for (std::uint32_t round = 1; round < 4096U; ++round) {
        material = hg::sha256_hex(digest);
        material.append(user_id);
        material.append(salt_hex(salt));
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

    AccessUser* target = nullptr;
    for (std::size_t i = 0; i < user_count_; ++i) {
        if (id == users_[i].id.data()) {
            target = &users_[i];
            break;
        }
    }
    if (target == nullptr) {
        if (user_count_ >= user_capacity) return false;
        target = &users_[user_count_++];
    }

    *target = AccessUser{};
    copy_text(target->id.data(), target->id.size(), id);
    copy_text(target->name.data(), target->name.size(), name);
    target->role = role;
    target->enabled = enabled;
    target->salt = salt;
    target->pin_digest = derive_pin_digest(id, pin, salt);
    return true;
}

const AccessUser* AccessControl::find_user(std::string_view id) const {
    for (std::size_t i = 0; i < user_count_; ++i) {
        if (id == users_[i].id.data()) return &users_[i];
    }
    return nullptr;
}

bool AccessControl::verify_pin(const AccessUser& user, std::string_view pin) const {
    if (!user.enabled || pin.empty()) return false;
    return hg::constant_time_equal(user.pin_digest, derive_pin_digest(user.id.data(), pin, user.salt));
}

bool AccessControl::role_allows(AccessRole role, std::string_view command) const {
    if (role == AccessRole::Admin) return true;
    if (role == AccessRole::Guest) return false;

    return command == "security.arm_home" ||
           command == "security.arm_away" ||
           command == "security.disarm" ||
           command == "light.set" ||
           command == "valve.close";
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

AuditDecision AccessControl::authorize(
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
        default: return "denied_unknown_user";
    }
}

}  // namespace homeguard
