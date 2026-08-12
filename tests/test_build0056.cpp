#include "test_framework.hpp"
#include "homeguard/access_control.hpp"

#include <array>
#include <cstdint>

void test_build0056() {
    using homeguard::AccessControl;
    using homeguard::AccessRole;
    using homeguard::AuditDecision;

    AccessControl access;
    std::array<std::uint8_t, 16> salt_admin{};
    std::array<std::uint8_t, 16> salt_user{};
    std::array<std::uint8_t, 16> salt_guest{};
    salt_admin[0] = 0x11U;
    salt_user[0] = 0x22U;
    salt_guest[0] = 0x33U;

    CHECK(access.set_user("admin", "Administrator", AccessRole::Admin, "1234", salt_admin));
    CHECK(access.set_user("user", "Operator", AccessRole::User, "2345", salt_user));
    CHECK(access.set_user("guest", "Guest", AccessRole::Guest, "3456", salt_guest));

    // Login validates identity independent of command role and is audited.
    CHECK(access.authenticate("admin", "1234") == AuditDecision::Allowed);
    CHECK(access.authenticate("user", "2345") == AuditDecision::Allowed);
    CHECK(access.authenticate("guest", "3456") == AuditDecision::Allowed);
    CHECK(access.authenticate("guest", "0000") == AuditDecision::DeniedCredential);
    CHECK(access.authenticate("missing", "1234") == AuditDecision::DeniedUnknownUser);

    // Destructive commissioning reset must be admin-only at the policy layer.
    CHECK(access.role_allows(AccessRole::Admin, "system.service.invalidate"));
    CHECK(!access.role_allows(AccessRole::User, "system.service.invalidate"));
    CHECK(!access.role_allows(AccessRole::Guest, "system.service.invalidate"));

    CHECK(access.authorize("admin", "1234", "system.service.invalidate") == AuditDecision::Allowed);
    CHECK(access.authorize("user", "2345", "system.service.invalidate") == AuditDecision::DeniedRole);
    CHECK(access.authorize("guest", "3456", "system.service.invalidate") == AuditDecision::DeniedRole);
    CHECK(access.authorize("admin", "9999", "system.service.invalidate") == AuditDecision::DeniedCredential);

    // Regression guard: normal operator permissions remain unchanged.
    CHECK(access.authorize("user", "2345", "security.arm_home") == AuditDecision::Allowed);
    CHECK(access.authorize("user", "2345", "security.disarm") == AuditDecision::Allowed);
    CHECK(access.authorize("user", "2345", "valve.open") == AuditDecision::Allowed);
    CHECK(access.authorize("user", "2345", "valve.close") == AuditDecision::Allowed);

    // Guest remains strictly read-only for every command path.
    CHECK(access.authorize("guest", "3456", "security.arm_home") == AuditDecision::DeniedRole);
    CHECK(access.authorize("guest", "3456", "valve.open") == AuditDecision::DeniedRole);

    CHECK(access.audit_size() == 15U);
}
