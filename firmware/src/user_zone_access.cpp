#include "homeguard/user_zone_access.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>

namespace homeguard {
namespace {
constexpr std::size_t kNotFound = std::numeric_limits<std::size_t>::max();

std::string_view id_view(const std::array<char, 24>& value)
{
    std::size_t length = 0;
    while (length < value.size() && value[length] != '\0') ++length;
    return {value.data(), length};
}
}

void UserZoneAccess::copy_id(std::array<char, 24>& destination, std::string_view source)
{
    destination.fill('\0');
    const auto length = std::min(source.size(), destination.size() - 1U);
    std::copy_n(source.begin(), length, destination.begin());
}

std::size_t UserZoneAccess::find_index(std::string_view user_id) const
{
    for (std::size_t i = 0; i < user_count_; ++i) {
        if (id_view(users_[i].user_id) == user_id) return i;
    }
    return kNotFound;
}

bool UserZoneAccess::ensure_user(std::string_view user_id)
{
    if (user_id.empty() || user_id.size() >= 24) return false;
    if (find_index(user_id) != kNotFound) return true;
    if (user_count_ >= user_capacity) return false;
    auto& record = users_[user_count_++];
    record = {};
    copy_id(record.user_id, user_id);
    for (auto& rule : record.zones) rule = ZoneAccessRule{};
    return true;
}

bool UserZoneAccess::remove_user(std::string_view user_id)
{
    const auto index = find_index(user_id);
    if (index == kNotFound) return false;
    for (std::size_t i = index + 1; i < user_count_; ++i) users_[i - 1] = users_[i];
    users_[--user_count_] = {};
    return true;
}

bool UserZoneAccess::set_rule(std::string_view user_id, std::uint16_t zone_id, ZoneAccessRule rule_value)
{
    if (zone_id == 0 || zone_id > zone_capacity) return false;
    if (!ensure_user(user_id)) return false;
    const auto index = find_index(user_id);
    if (index == kNotFound) return false;
    users_[index].zones[zone_id - 1U] = rule_value;
    return true;
}

const ZoneAccessRule* UserZoneAccess::rule(std::string_view user_id, std::uint16_t zone_id) const
{
    if (zone_id == 0 || zone_id > zone_capacity) return nullptr;
    const auto index = find_index(user_id);
    return index == kNotFound ? nullptr : &users_[index].zones[zone_id - 1U];
}

const UserZoneAccessRecord* UserZoneAccess::user_at(std::size_t index) const
{
    return index < user_count_ ? &users_[index] : nullptr;
}

}  // namespace homeguard
