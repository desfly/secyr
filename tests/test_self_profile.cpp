#include "test_framework.hpp"
#include "homeguard/access_control.hpp"
#include "homeguard/self_profile.hpp"

#include <array>
#include <string>

void test_self_profile() {
    homeguard::AccessControl access{};
    std::array<std::uint8_t, 16> salt{};
    salt[0] = 0x42;

    CHECK(access.set_user("guest1", "Guest One", homeguard::AccessRole::Guest, "1234", salt, true));
    CHECK(access.set_user("user1", "User One", homeguard::AccessRole::User, "2345", salt, true));
    CHECK(access.set_user("admin", "Administrator", homeguard::AccessRole::Admin, "3456", salt, true));

    homeguard::SelfProfile profile{};
    CHECK(homeguard::build_self_profile(access, "guest1", profile));
    CHECK(std::string(profile.id.data()) == "guest1");
    CHECK(std::string(profile.name.data()) == "Guest One");
    CHECK(profile.role == homeguard::AccessRole::Guest);
    CHECK(profile.enabled);
    CHECK(!profile.can_arm);
    CHECK(!profile.can_disarm);
    CHECK(!profile.can_control_valves);
    CHECK(!profile.can_manage_users);

    CHECK(homeguard::build_self_profile(access, "user1", profile));
    CHECK(std::string(profile.id.data()) == "user1");
    CHECK(profile.role == homeguard::AccessRole::User);
    CHECK(profile.can_arm);
    CHECK(profile.can_disarm);
    CHECK(profile.can_control_valves);
    CHECK(!profile.can_manage_users);

    CHECK(homeguard::build_self_profile(access, "admin", profile));
    CHECK(profile.role == homeguard::AccessRole::Admin);
    CHECK(profile.can_manage_users);

    CHECK(!homeguard::build_self_profile(access, "missing", profile));
}
