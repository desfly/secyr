#include "test_framework.hpp"
#include "homeguard/access_control.hpp"

void test_access_permissions() {
    using homeguard::AccessControl;
    using homeguard::AccessRole;

    AccessControl access;

    CHECK(AccessControl::user_capacity == 8U);

    // Admin is the only role allowed to perform configuration, service,
    // user-management, panic, or destructive maintenance commands.
    CHECK(access.role_allows(AccessRole::Admin, "security.arm_home"));
    CHECK(access.role_allows(AccessRole::Admin, "security.disarm"));
    CHECK(access.role_allows(AccessRole::Admin, "security.panic"));
    CHECK(access.role_allows(AccessRole::Admin, "valve.open"));
    CHECK(access.role_allows(AccessRole::Admin, "valve.close"));
    CHECK(access.role_allows(AccessRole::Admin, "light.set"));
    CHECK(access.role_allows(AccessRole::Admin, "valve.clear_latch"));
    CHECK(access.role_allows(AccessRole::Admin, "network.configure"));
    CHECK(access.role_allows(AccessRole::Admin, "cloud.configure"));
    CHECK(access.role_allows(AccessRole::Admin, "access.manage"));
    CHECK(access.role_allows(AccessRole::Admin, "system.service.invalidate"));
    CHECK(access.role_allows(AccessRole::Admin, "system.factory_reset"));
    CHECK(access.role_allows(AccessRole::Admin, "system.reboot"));

    // Normal users may operate the security partition and valves only.
    CHECK(access.role_allows(AccessRole::User, "security.arm_home"));
    CHECK(access.role_allows(AccessRole::User, "security.arm_away"));
    CHECK(access.role_allows(AccessRole::User, "security.disarm"));
    CHECK(access.role_allows(AccessRole::User, "valve.open"));
    CHECK(access.role_allows(AccessRole::User, "valve.close"));
    CHECK(!access.role_allows(AccessRole::User, "security.panic"));
    CHECK(!access.role_allows(AccessRole::User, "light.set"));
    CHECK(!access.role_allows(AccessRole::User, "valve.clear_latch"));
    CHECK(!access.role_allows(AccessRole::User, "network.configure"));
    CHECK(!access.role_allows(AccessRole::User, "cloud.configure"));
    CHECK(!access.role_allows(AccessRole::User, "access.manage"));
    CHECK(!access.role_allows(AccessRole::User, "system.service.invalidate"));
    CHECK(!access.role_allows(AccessRole::User, "system.factory_reset"));
    CHECK(!access.role_allows(AccessRole::User, "system.reboot"));

    // Guest is monitor-only: no command path is granted by RBAC.
    CHECK(!access.role_allows(AccessRole::Guest, "security.arm_home"));
    CHECK(!access.role_allows(AccessRole::Guest, "security.arm_away"));
    CHECK(!access.role_allows(AccessRole::Guest, "security.disarm"));
    CHECK(!access.role_allows(AccessRole::Guest, "security.panic"));
    CHECK(!access.role_allows(AccessRole::Guest, "valve.open"));
    CHECK(!access.role_allows(AccessRole::Guest, "valve.close"));
    CHECK(!access.role_allows(AccessRole::Guest, "light.set"));
    CHECK(!access.role_allows(AccessRole::Guest, "network.configure"));
    CHECK(!access.role_allows(AccessRole::Guest, "access.manage"));
    CHECK(!access.role_allows(AccessRole::Guest, "system.factory_reset"));
}
