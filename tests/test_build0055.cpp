#include "test_framework.hpp"
#include "homeguard/access_control.hpp"

void test_build0055() {
    using homeguard::AccessControl;
    using homeguard::AccessRole;

    AccessControl access;

    CHECK(AccessControl::user_capacity == 8U);

    CHECK(access.role_allows(AccessRole::Admin, "security.arm_home"));
    CHECK(access.role_allows(AccessRole::Admin, "security.disarm"));
    CHECK(access.role_allows(AccessRole::Admin, "valve.open"));
    CHECK(access.role_allows(AccessRole::Admin, "valve.close"));
    CHECK(access.role_allows(AccessRole::Admin, "light.set"));
    CHECK(access.role_allows(AccessRole::Admin, "valve.clear_latch"));

    CHECK(access.role_allows(AccessRole::User, "security.arm_home"));
    CHECK(access.role_allows(AccessRole::User, "security.arm_away"));
    CHECK(access.role_allows(AccessRole::User, "security.disarm"));
    CHECK(access.role_allows(AccessRole::User, "valve.open"));
    CHECK(access.role_allows(AccessRole::User, "valve.close"));
    CHECK(!access.role_allows(AccessRole::User, "light.set"));
    CHECK(!access.role_allows(AccessRole::User, "valve.clear_latch"));
    CHECK(!access.role_allows(AccessRole::User, "system.reboot"));

    CHECK(!access.role_allows(AccessRole::Guest, "security.arm_home"));
    CHECK(!access.role_allows(AccessRole::Guest, "security.disarm"));
    CHECK(!access.role_allows(AccessRole::Guest, "valve.open"));
    CHECK(!access.role_allows(AccessRole::Guest, "valve.close"));
    CHECK(!access.role_allows(AccessRole::Guest, "light.set"));
}
