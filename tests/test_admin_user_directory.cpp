#include "test_framework.hpp"
#include "homeguard/admin_user_directory.hpp"

#include <array>
#include <string>

void test_admin_user_directory() {
    homeguard::AccessControl access{};
    std::array<std::uint8_t, 16> salt{};

    CHECK(access.set_user("admin", "Admin", homeguard::AccessRole::Admin, "1111", salt, true));
    CHECK(access.set_user("user1", "User One", homeguard::AccessRole::User, "2222", salt, true));
    CHECK(access.set_user("guest1", "Guest One", homeguard::AccessRole::Guest, "3333", salt, true));

    homeguard::AdminUserDirectory directory{};
    CHECK(!homeguard::build_admin_user_directory(access, "user1", directory));
    CHECK(directory.count == 0U);
    CHECK(!homeguard::build_admin_user_directory(access, "guest1", directory));
    CHECK(directory.count == 0U);
    CHECK(!homeguard::build_admin_user_directory(access, "missing", directory));
    CHECK(directory.count == 0U);

    CHECK(homeguard::build_admin_user_directory(access, "admin", directory));
    CHECK(directory.count == 3U);
    CHECK(std::string(directory.users[0].id.data()) == "admin");
    CHECK(std::string(directory.users[1].id.data()) == "user1");
    CHECK(std::string(directory.users[2].id.data()) == "guest1");
}
