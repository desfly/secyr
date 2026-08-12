#include "homeguard/user_zone_access.hpp"

#include <algorithm>

namespace homeguard {
namespace {
constexpr std::size_t npos = static_cast<std::size_t>(-1);
}

void UserZoneAccess::copy_id(std::array<char, 24>& destination, std::string_view source) {
    destination.fill('\0');
    const auto count = std::min(source.size(), destination.size() - 1U);
    std::copy_n(source.begin(), count, destination.begin());
}

std::size_t UserZoneAccess::find_index(std::string_view user_id) const {
    for (std::size_t i = 0; i < user_count_; ++i) {
        if (std::string_view{users_[i].user_id.data()} == user_id) return i;
    }
    return npos;
}

bool UserZoneAccess::ensure_user(std::string_view user_id) {
    if (user_id.empty() || user_id.size() >= users_[0].user_id.size()) return false;
    if (find_index(user_id) != npos) return true;
    if (user_count_ >= user_capacity) return false;
    auto& record = users_[user_count_++];
    copy_id(record.user_id, user_id);
    for (auto& rule : record.zones) rule = ZoneAccessRule{};
    return true;
}

bool UserZoneAccess::remove_user(std::string_view user_id) {
    const auto index = find_index(user_id);
    if (index == npos) return false;
    for (std::size_t i = index + 1U; i < user_count_; ++i) users_[i - 1U] = users_[i];
    users_[--user_count_] = UserZoneAccessRecord{};
    return true;
}

bool UserZoneAccess::set_rule(std::string_view user_id, std::uint16_t zone_id, ZoneAccessRule rule_value) {
    if (zone_id == 0 || zone_id > zone_capacity) return false;
    if (!ensure_user(user_id)) return false;
    const auto index = find_index(user_id);
    if (index == npos) return false;
    users_[index].zones[zone_id - 1U] = rule_value;
    return true;
}

const ZoneAccessRule* UserZoneAccess::rule(std::string_view user_id, std::uint16_t zone_id) const {
    if (zone_id == 0 || zone_id > zone_capacity) return nullptr;
    const auto index = find_index(user_id);
    if (index == npos) return nullptr;
    return &users_[index].zones[zone_id - 1U];
}

const UserZoneAccessRecord* UserZoneAccess::user_at(std::size_t index) const {
    return index < user_count_ ? &users_[index] : nullptr;
}

}  // namespace homeguard
