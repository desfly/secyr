#include "test_framework.hpp"
#include "homeguard/access_control.hpp"

void test_access_privacy() {
    homeguard::AccessControl access{};

    CHECK(access.role_allows(homeguard::AccessRole::Admin, "access.users.list"));
    CHECK(access.role_allows(homeguard::AccessRole::Admin, "access.users.manage"));
    CHECK(access.role_allows(homeguard::AccessRole::Admin, "access.config.export"));

    CHECK(!access.role_allows(homeguard::AccessRole::User, "access.users.list"));
    CHECK(!access.role_allows(homeguard::AccessRole::User, "access.users.manage"));
    CHECK(!access.role_allows(homeguard::AccessRole::User, "access.config.export"));

    CHECK(!access.role_allows(homeguard::AccessRole::Guest, "access.users.list"));
    CHECK(!access.role_allows(homeguard::AccessRole::Guest, "access.users.manage"));
    CHECK(!access.role_allows(homeguard::AccessRole::Guest, "access.config.export"));

    CHECK(access.role_allows(homeguard::AccessRole::Admin, "profile.self"));
    CHECK(access.role_allows(homeguard::AccessRole::User, "profile.self"));
    CHECK(access.role_allows(homeguard::AccessRole::Guest, "profile.self"));

    CHECK(access.role_allows(homeguard::AccessRole::User, "system.status"));
    CHECK(access.role_allows(homeguard::AccessRole::User, "zones.list"));
    CHECK(access.role_allows(homeguard::AccessRole::User, "sensors.list"));
    CHECK(access.role_allows(homeguard::AccessRole::User, "security.arm_home"));
    CHECK(access.role_allows(homeguard::AccessRole::User, "security.disarm"));
    CHECK(access.role_allows(homeguard::AccessRole::User, "valve.open"));
    CHECK(access.role_allows(homeguard::AccessRole::User, "valve.close"));

    CHECK(access.role_allows(homeguard::AccessRole::Guest, "sensors.status"));
    CHECK(access.role_allows(homeguard::AccessRole::Guest, "sensors.list"));
    CHECK(!access.role_allows(homeguard::AccessRole::Guest, "system.status"));
    CHECK(!access.role_allows(homeguard::AccessRole::Guest, "status"));
    CHECK(!access.role_allows(homeguard::AccessRole::Guest, "zones.status"));
    CHECK(!access.role_allows(homeguard::AccessRole::Guest, "zones.list"));
    CHECK(!access.role_allows(homeguard::AccessRole::Guest, "security.arm_home"));
    CHECK(!access.role_allows(homeguard::AccessRole::Guest, "security.disarm"));
    CHECK(!access.role_allows(homeguard::AccessRole::Guest, "valve.open"));
    CHECK(!access.role_allows(homeguard::AccessRole::Guest, "valve.close"));
}
