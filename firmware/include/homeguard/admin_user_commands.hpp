#pragma once

#include "homeguard/access_control.hpp"
#include "homeguard/admin_user_directory.hpp"

#include <string>
#include <string_view>

namespace homeguard {

enum class AdminUserCommandStatus {
    Ok,
    Denied,
    NotFound,
    LastAdminProtected,
    InvalidRequest
};

struct AdminUserCommandResult {
    AdminUserCommandStatus status{AdminUserCommandStatus::InvalidRequest};
    AdminUserDirectory directory{};
};

[[nodiscard]] AdminUserCommandResult admin_users_list(
    const AccessControl& access,
    std::string_view requester_id);

[[nodiscard]] AdminUserCommandResult admin_user_set_enabled(
    AccessControl& access,
    std::string_view requester_id,
    std::string_view target_id,
    bool enabled);

[[nodiscard]] AdminUserCommandResult admin_user_delete(
    AccessControl& access,
    std::string_view requester_id,
    std::string_view target_id);

[[nodiscard]] const char* to_string(AdminUserCommandStatus status) noexcept;

}  // namespace homeguard
