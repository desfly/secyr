#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace homeguard {

struct ZoneAccessRule {
    bool visible{true};
    bool can_arm{false};
    bool can_disarm{false};
    bool can_bypass{false};
};

struct UserZoneAccessRecord {
    std::array<char, 24> user_id{};
    std::array<ZoneAccessRule, 16> zones{};
};

class UserZoneAccess {
public:
    static constexpr std::size_t user_capacity = 8;
    static constexpr std::size_t zone_capacity = 16;

    bool ensure_user(std::string_view user_id);
    bool remove_user(std::string_view user_id);
    bool set_rule(std::string_view user_id, std::uint16_t zone_id, ZoneAccessRule rule);
    [[nodiscard]] const ZoneAccessRule* rule(std::string_view user_id, std::uint16_t zone_id) const;
    [[nodiscard]] const UserZoneAccessRecord* user_at(std::size_t index) const;
    [[nodiscard]] std::size_t user_count() const { return user_count_; }

private:
    static void copy_id(std::array<char, 24>& destination, std::string_view source);
    [[nodiscard]] std::size_t find_index(std::string_view user_id) const;

    std::array<UserZoneAccessRecord, user_capacity> users_{};
    std::size_t user_count_{};
};

}  // namespace homeguard
