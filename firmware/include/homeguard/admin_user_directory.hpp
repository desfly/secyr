#pragma once

#include "homeguard/access_control.hpp"

#include <array>
#include <cstddef>
#include <string_view>

namespace homeguard {

struct AdminUserView {
    std::array<char, 24> id{};
    std::array<char, 32> name{};
    AccessRole role{AccessRole::Guest};
    bool enabled{false};
};

struct AdminUserDirectory {
    std::array<AdminUserView, AccessControl::user_capacity> users{};
    std::size_t count{0};
};

[[nodiscard]] bool build_admin_user_directory(
    const AccessControl& access,
    std::string_view requester_id,
    AdminUserDirectory& out);

}  // namespace homeguard
