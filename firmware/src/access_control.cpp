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

bool terminated(const auto& text) {
    return std::find(text.begin(), text.end(), '\0') != text.end();
}

bool constant_time_text_equal(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) return false;
    unsigned char diff = 0U;
    for (std::size_t i = 0; i < left.size(); ++i) {
        diff |= static_cast<unsigned char>(left[i]) ^ static_cast<unsigned char>(right[i]);
    }
    return diff == 0U;
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

    AccessUser record{};
    copy_text(record.id.data(), record.id.size(), id);
    copy_text(record.name.data(), record.name.size(), name);
    record.role = role;
    record.enabled = enabled;
    record.salt = salt;
    record.pin_digest = derive_pin_digest(id, pin, salt);
    return import_user(record);
}

bool AccessControl::import_user(const AccessUser& user) {
    if (!terminated(user.id) || !terminated(user.name) || user.id[0] == '\0') return false;
    if (static_cast<std::uint8_t>(user.role) > static_cast<std::uint8_t>(AccessRole::Admin)) return false;

    AccessUser* target = nullptr;
    const std::string_view id{user.id.data()};
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
    *target = user;
    return true;
}

void AccessControl::clear_users() {
    for (auto& user : users_) user = AccessUser{};
    user_count_ = 0;
}

const AccessUser* AccessControl::user_at(std::size_t index) const {
    return index < user_count_ ? &users_[index] : nullptr;
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

    // Standard User role is intentionally narrow:
    // - monitoring/status reads;
    // - arm home / arm away;
    // - disarm.
    // No service/configuration/output/user-management commands are allowed.
    return command == "system.status" || command == "status" ||
           command == "security.arm_home" || command == "arm_home" ||
           command == "security.arm_away" || command == "arm_away" ||
           command == "security.disarm" || command == "disarm";
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

AuditDecision AccessControl::authorize_cloud_proof(
    std::string_view actor,
    std::string_view command,
    std::string_view nonce,
    std::string_view request_id,
    std::string_view proof_hex) {
    const auto* user = find_user(actor);
    if (user == nullptr || !user->enabled) {
        append_audit(actor, command, AuditDecision::DeniedUnknownUser);
        return AuditDecision::DeniedUnknownUser;
    }
    if (!role_allows(user->role, command)) {
        append_audit(actor, command, AuditDecision::DeniedRole);
        return AuditDecision::DeniedRole;
    }
    if (nonce.empty() || request_id.empty() || proof_hex.size() != 64U) {
        append_audit(actor, command, AuditDecision::DeniedCredential);
        return AuditDecision::DeniedCredential;
    }

    std::string material{"HomeGuard-S3|CLOUD|"};
    material += hg::sha256_hex(user->pin_digest);
    material.push_back('|');
    material.append(nonce);
    material.push_back('|');
    material.append(request_id);
    material.push_back('|');
    material.append(command);
    const auto expected = hg::sha256_hex(hg::sha256(material));
    if (!constant_time_text_equal(expected, proof_hex)) {
        append_audit(actor, command, AuditDecision::DeniedCredential);
        return AuditDecision::DeniedCredential;
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
