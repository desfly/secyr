#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace homeguard {

struct OutputAccessRule {
    bool visible{true};
    bool can_on{false};
    bool can_off{false};
};

struct UserOutputAccessRecord {
    std::array<char, 24> user_id{};
    std::array<OutputAccessRule, 16> outputs{};
};

class UserOutputAccess {
public:
    static constexpr std::size_t user_capacity = 8;
    static constexpr std::size_t output_capacity = 16;

    bool ensure_user(std::string_view user_id);
    bool remove_user(std::string_view user_id);
    bool set_rule(std::string_view user_id, std::uint16_t output_id, OutputAccessRule rule);
    [[nodiscard]] const OutputAccessRule* rule(std::string_view user_id, std::uint16_t output_id) const;
    [[nodiscard]] const UserOutputAccessRecord* user_at(std::size_t index) const;
    [[nodiscard]] std::size_t user_count() const { return user_count_; }

private:
    static void copy_id(std::array<char, 24>& destination, std::string_view source);
    [[nodiscard]] std::size_t find_index(std::string_view user_id) const;

    std::array<UserOutputAccessRecord, user_capacity> users_{};
    std::size_t user_count_{};
};

}  // namespace homeguard
