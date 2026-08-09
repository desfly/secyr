#include "test_framework.hpp"
#include "homeguard/access_control.hpp"
#include "homeguard/admin_user_commands.hpp"

#include <array>

void test_admin_user_commands() {
    homeguard::AccessControl access{};
    std::array<std::uint8_t, 16> salt{};

    CHECK(access.set_user("admin", "Admin", homeguard::AccessRole::Admin, "1111", salt, true));
    CHECK(access.set_user("user1", "User One", homeguard::AccessRole::User, "2222", salt, true));
    CHECK(access.set_user("guest1", "Guest One", homeguard::AccessRole::Guest, "3333", salt, true));

    auto result = homeguard::admin_users_list(access, "admin");
    CHECK(result.status == homeguard::AdminUserCommandStatus::Ok);
    CHECK(result.directory.count == 3U);

    result = homeguard::admin_users_list(access, "user1");
    CHECK(result.status == homeguard::AdminUserCommandStatus::Denied);
    CHECK(result.directory.count == 0U);

    result = homeguard::admin_users_list(access, "guest1");
    CHECK(result.status == homeguard::AdminUserCommandStatus::Denied);
    CHECK(result.directory.count == 0U);

    result = homeguard::admin_user_set_enabled(access, "admin", "user1", false);
    CHECK(result.status == homeguard::AdminUserCommandStatus::Ok);
    CHECK(!access.find_user("user1")->enabled);

    result = homeguard::admin_user_delete(access, "admin", "guest1");
    CHECK(result.status == homeguard::AdminUserCommandStatus::Ok);
    CHECK(access.find_user("guest1") == nullptr);

    result = homeguard::admin_user_set_enabled(access, "admin", "admin", false);
    CHECK(result.status == homeguard::AdminUserCommandStatus::LastAdminProtected);
    CHECK(access.find_user("admin")->enabled);

    result = homeguard::admin_user_delete(access, "admin", "admin");
    CHECK(result.status == homeguard::AdminUserCommandStatus::LastAdminProtected);
    CHECK(access.find_user("admin") != nullptr);
}
