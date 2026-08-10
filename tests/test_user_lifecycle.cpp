#include "test_framework.hpp"
#include "homeguard/access_control.hpp"

#include <array>
#include <string>

void test_user_lifecycle() {
    homeguard::AccessControl access{};
    std::array<std::uint8_t, 16> salt{};

    CHECK(access.set_user("admin", "Admin", homeguard::AccessRole::Admin, "1111", salt, true));
    CHECK(access.set_user("user1", "User 1", homeguard::AccessRole::User, "2222", salt, true));
    CHECK(access.set_user("guest1", "Guest 1", homeguard::AccessRole::Guest, "3333", salt, true));

    // Ordinary accounts may be disabled and removed.
    CHECK(access.set_user_enabled("user1", false));
    CHECK(access.find_user("user1") != nullptr);
    CHECK(!access.find_user("user1")->enabled);
    CHECK(access.remove_user("guest1"));
    CHECK(access.find_user("guest1") == nullptr);

    // The last enabled administrator is fail-safe protected.
    CHECK(!access.set_user_enabled("admin", false));
    CHECK(access.find_user("admin") != nullptr);
    CHECK(access.find_user("admin")->enabled);
    CHECK(!access.remove_user("admin"));
    CHECK(access.find_user("admin") != nullptr);

    // With a second enabled administrator, one administrator may be disabled.
    CHECK(access.set_user("admin2", "Admin 2", homeguard::AccessRole::Admin, "4444", salt, true));
    CHECK(access.set_user_enabled("admin", false));
    CHECK(access.find_user("admin2")->enabled);

    // Capacity is exactly eight total accounts.
    homeguard::AccessControl full{};
    for (int i = 0; i < 8; ++i) {
        const std::string id = "u" + std::to_string(i);
        const auto role = i == 0 ? homeguard::AccessRole::Admin : homeguard::AccessRole::User;
        CHECK(full.set_user(id, id, role, "5555", salt, true));
    }
    CHECK(full.user_count() == 8U);
    CHECK(!full.set_user("u8", "overflow", homeguard::AccessRole::Guest, "5555", salt, true));
    CHECK(full.user_count() == 8U);
}
