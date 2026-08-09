#include "homeguard/admin_user_commands.hpp"

namespace homeguard {
namespace {

bool requester_is_active_admin(const AccessControl& access, std::string_view requester_id)
{
    const auto* requester = access.find_user(requester_id);
    return requester != nullptr && requester->enabled && requester->role == AccessRole::Admin;
}

AdminUserCommandResult make_result(AdminUserCommandStatus status)
{
    AdminUserCommandResult result{};
    result.status = status;
    return result;
}

}

AdminUserCommandResult admin_users_list(
    const AccessControl& access,
    std::string_view requester_id)
{
    AdminUserCommandResult result{};
    if (!build_admin_user_directory(access, requester_id, result.directory)) {
        result.status = AdminUserCommandStatus::Denied;
        return result;
    }
    result.status = AdminUserCommandStatus::Ok;
    return result;
}

AdminUserCommandResult admin_user_set_enabled(
    AccessControl& access,
    std::string_view requester_id,
    std::string_view target_id,
    bool enabled)
{
    if (!requester_is_active_admin(access, requester_id)) {
        return make_result(AdminUserCommandStatus::Denied);
    }
    if (target_id.empty()) {
        return make_result(AdminUserCommandStatus::InvalidRequest);
    }
    if (access.find_user(target_id) == nullptr) {
        return make_result(AdminUserCommandStatus::NotFound);
    }

    const AccessControl backup = access;
    if (!access.set_user_enabled(target_id, enabled)) {
        access = backup;
        return make_result(AdminUserCommandStatus::LastAdminProtected);
    }

    auto result = admin_users_list(access, requester_id);
    if (result.status != AdminUserCommandStatus::Ok) {
        access = backup;
        return make_result(AdminUserCommandStatus::Denied);
    }
    return result;
}

AdminUserCommandResult admin_user_delete(
    AccessControl& access,
    std::string_view requester_id,
    std::string_view target_id)
{
    if (!requester_is_active_admin(access, requester_id)) {
        return make_result(AdminUserCommandStatus::Denied);
    }
    if (target_id.empty()) {
        return make_result(AdminUserCommandStatus::InvalidRequest);
    }
    if (access.find_user(target_id) == nullptr) {
        return make_result(AdminUserCommandStatus::NotFound);
    }

    const AccessControl backup = access;
    if (!access.remove_user(target_id)) {
        access = backup;
        return make_result(AdminUserCommandStatus::LastAdminProtected);
    }

    auto result = admin_users_list(access, requester_id);
    if (result.status != AdminUserCommandStatus::Ok) {
        access = backup;
        return make_result(AdminUserCommandStatus::Denied);
    }
    return result;
}

const char* to_string(AdminUserCommandStatus status) noexcept
{
    switch (status) {
        case AdminUserCommandStatus::Ok: return "ok";
        case AdminUserCommandStatus::Denied: return "denied_role";
        case AdminUserCommandStatus::NotFound: return "user_not_found";
        case AdminUserCommandStatus::LastAdminProtected: return "last_admin_protected";
        default: return "invalid_request";
    }
}

}  // namespace homeguard
