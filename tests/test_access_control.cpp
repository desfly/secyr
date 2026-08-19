#include "test_framework.hpp"
#include "homeguard/access_control.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace {
std::uint64_t g_auth_clock_ms = 0;
std::uint64_t fake_auth_clock() { return g_auth_clock_ms; }

void test_login_and_role_policy() {
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

    CHECK(access.authenticate("admin", "1234") == AuditDecision::Allowed);
    CHECK(access.authenticate("user", "2345") == AuditDecision::Allowed);
    CHECK(access.authenticate("guest", "3456") == AuditDecision::Allowed);
    CHECK(access.authenticate("guest", "0000") == AuditDecision::DeniedCredential);
    CHECK(access.authenticate("missing", "1234") == AuditDecision::DeniedUnknownUser);

    CHECK(access.role_allows(AccessRole::Admin, "system.service.invalidate"));
    CHECK(!access.role_allows(AccessRole::User, "system.service.invalidate"));
    CHECK(!access.role_allows(AccessRole::Guest, "system.service.invalidate"));

    CHECK(access.authorize("admin", "1234", "system.service.invalidate") == AuditDecision::Allowed);
    CHECK(access.authorize("user", "2345", "system.service.invalidate") == AuditDecision::DeniedRole);
    CHECK(access.authorize("guest", "3456", "system.service.invalidate") == AuditDecision::DeniedRole);
    CHECK(access.authorize("admin", "9999", "system.service.invalidate") == AuditDecision::DeniedCredential);

    CHECK(access.authorize("user", "2345", "security.arm_home") == AuditDecision::Allowed);
    CHECK(access.authorize("user", "2345", "security.disarm") == AuditDecision::Allowed);
    CHECK(access.authorize("user", "2345", "valve.open") == AuditDecision::Allowed);
    CHECK(access.authorize("user", "2345", "valve.close") == AuditDecision::Allowed);

    CHECK(access.authorize("guest", "3456", "security.arm_home") == AuditDecision::DeniedRole);
    CHECK(access.authorize("guest", "3456", "valve.open") == AuditDecision::DeniedRole);
    CHECK(access.audit_size() == 15U);
}

void test_admin_preservation() {
    using homeguard::AccessControl;
    using homeguard::AccessRole;

    AccessControl access;
    std::array<std::uint8_t, 16> salt1{};
    std::array<std::uint8_t, 16> salt2{};
    std::array<std::uint8_t, 16> salt3{};
    salt1[0] = 0x11U;
    salt2[0] = 0x22U;
    salt3[0] = 0x33U;

    CHECK(access.enabled_admin_count() == 0U);
    CHECK(!access.would_preserve_admin_access("operator", AccessRole::User, true));
    CHECK(!access.would_preserve_admin_access("guest", AccessRole::Guest, true));
    CHECK(access.would_preserve_admin_access("admin1", AccessRole::Admin, true));

    CHECK(access.set_user("admin1", "Primary Admin", AccessRole::Admin, "1234", salt1, true));
    CHECK(access.enabled_admin_count() == 1U);
    CHECK(!access.would_preserve_admin_access("admin1", AccessRole::Admin, false));
    CHECK(!access.would_preserve_admin_access("admin1", AccessRole::User, true));
    CHECK(!access.would_preserve_admin_access("admin1", AccessRole::Guest, true));
    CHECK(access.would_preserve_admin_access("admin1", AccessRole::Admin, true));

    CHECK(access.would_preserve_admin_access("operator", AccessRole::User, true));
    CHECK(access.set_user("operator", "Operator", AccessRole::User, "2345", salt2, true));
    CHECK(access.enabled_admin_count() == 1U);

    CHECK(access.would_preserve_admin_access("admin2", AccessRole::Admin, true));
    CHECK(access.set_user("admin2", "Backup Admin", AccessRole::Admin, "3456", salt3, true));
    CHECK(access.enabled_admin_count() == 2U);
    CHECK(access.would_preserve_admin_access("admin1", AccessRole::User, true));
    CHECK(access.set_user("admin1", "Primary Admin", AccessRole::User, "1234", salt1, true));
    CHECK(access.enabled_admin_count() == 1U);

    CHECK(!access.would_preserve_admin_access("admin2", AccessRole::Admin, false));
    CHECK(!access.would_preserve_admin_access("admin2", AccessRole::Guest, true));

    CHECK(access.would_preserve_admin_access("admin1", AccessRole::Admin, true));
    CHECK(access.set_user("admin1", "Primary Admin", AccessRole::Admin, "1234", salt1, true));
    CHECK(access.enabled_admin_count() == 2U);
    CHECK(access.would_preserve_admin_access("admin2", AccessRole::Admin, false));
}

void test_rate_limit_and_capacity() {
    using homeguard::AccessControl;
    using homeguard::AccessRole;
    using homeguard::AuditDecision;

    AccessControl access;
    std::array<std::uint8_t, 16> admin_salt{};
    std::array<std::uint8_t, 16> user_salt{};
    admin_salt[0] = 0x58U;
    user_salt[0] = 0x59U;
    CHECK(access.set_user("admin", "Administrator", AccessRole::Admin, "1234", admin_salt));
    CHECK(access.set_user("user", "Operator", AccessRole::User, "2345", user_salt));

    CHECK(access.authenticate("admin", "0000", 1000) == AuditDecision::DeniedCredential);
    CHECK(access.authentication_retry_after_ms("admin", 1000) == 250U);
    CHECK(access.authenticate("admin", "1234", 1100) == AuditDecision::DeniedRateLimited);
    CHECK(access.authentication_retry_after_ms("admin", 1100) == 150U);
    CHECK(access.authenticate("admin", "1234", 1250) == AuditDecision::Allowed);
    CHECK(access.authentication_retry_after_ms("admin", 1250) == 0U);

    CHECK(access.authenticate("admin", "0000", 2000) == AuditDecision::DeniedCredential);
    CHECK(access.authentication_retry_after_ms("admin", 2000) == 250U);
    CHECK(access.authenticate("admin", "0000", 2100) == AuditDecision::DeniedRateLimited);
    CHECK(access.authentication_retry_after_ms("admin", 2100) == 150U);
    CHECK(access.authenticate("admin", "0000", 2250) == AuditDecision::DeniedCredential);
    CHECK(access.authentication_retry_after_ms("admin", 2250) == 500U);
    CHECK(access.authorize("admin", "1234", "access.manage", 2500) == AuditDecision::DeniedRateLimited);
    CHECK(access.authorize("admin", "1234", "access.manage", 2750) == AuditDecision::Allowed);
    CHECK(access.authentication_retry_after_ms("admin", 2750) == 0U);

    CHECK(access.authorize("user", "9999", "security.arm_home", 3000) == AuditDecision::DeniedCredential);
    CHECK(access.authenticate("user", "2345", 3100) == AuditDecision::DeniedRateLimited);
    CHECK(access.authorize("user", "2345", "security.arm_home", 3250) == AuditDecision::Allowed);
    CHECK(access.authorize("user", "9999", "system.network.configure", 4000) == AuditDecision::DeniedCredential);
    CHECK(access.authorize("user", "2345", "system.network.configure", 4250) == AuditDecision::DeniedRole);
    CHECK(access.authentication_retry_after_ms("user", 4250) == 0U);

    CHECK(access.authenticate("missing-a", "1111", 5000) == AuditDecision::DeniedUnknownUser);
    CHECK(access.authenticate("missing-b", "1111", 5100) == AuditDecision::DeniedRateLimited);
    CHECK(access.authentication_retry_after_ms("anything-unknown", 5100) == 150U);
    CHECK(access.authenticate("missing-c", "1111", 5250) == AuditDecision::DeniedUnknownUser);
    CHECK(access.authentication_retry_after_ms("missing-c", 5250) == 500U);

    AccessControl runtime_access;
    std::array<std::uint8_t, 16> runtime_salt{};
    runtime_salt[0] = 0x5aU;
    CHECK(runtime_access.set_user("runtime", "Runtime Admin", AccessRole::Admin, "6789", runtime_salt));
    CHECK(!runtime_access.auth_throttle_enabled());
    runtime_access.set_auth_clock(&fake_auth_clock);
    CHECK(runtime_access.auth_throttle_enabled());
    g_auth_clock_ms = 10000;
    CHECK(runtime_access.authorize("runtime", "0000", "system.service.invalidate") == AuditDecision::DeniedCredential);
    g_auth_clock_ms = 10100;
    CHECK(runtime_access.authorize("runtime", "6789", "system.service.invalidate") == AuditDecision::DeniedRateLimited);
    g_auth_clock_ms = 10250;
    CHECK(runtime_access.authorize("runtime", "6789", "system.service.invalidate") == AuditDecision::Allowed);

    AccessControl matrix;
    for (std::size_t i = 0; i < AccessControl::user_capacity; ++i) {
        std::array<std::uint8_t, 16> salt{};
        salt[0] = static_cast<std::uint8_t>(0x70U + i);
        const auto role = i < 2U ? AccessRole::Admin : (i < 6U ? AccessRole::User : AccessRole::Guest);
        const char* ids[] = {"admin1", "admin2", "user1", "user2", "user3", "user4", "guest1", "guest2"};
        CHECK(matrix.set_user(ids[i], ids[i], role, "2468", salt));
    }
    CHECK(matrix.user_count() == AccessControl::user_capacity);
    std::array<std::uint8_t, 16> extra_salt{};
    CHECK(!matrix.set_user("ninth", "Ninth", AccessRole::Guest, "2468", extra_salt));

    for (std::size_t i = 0; i < AccessControl::user_capacity; ++i) {
        const auto* u = matrix.user_at(i);
        CHECK(u != nullptr);
        CHECK(matrix.authenticate(u->id.data(), "2468") == AuditDecision::Allowed);
        const bool admin = u->role == AccessRole::Admin;
        const bool user = u->role == AccessRole::User;
        CHECK(matrix.role_allows(u->role, "security.arm_home") == (admin || user));
        CHECK(matrix.role_allows(u->role, "security.arm_away") == (admin || user));
        CHECK(matrix.role_allows(u->role, "security.disarm") == (admin || user));
        CHECK(matrix.role_allows(u->role, "valve.open") == (admin || user));
        CHECK(matrix.role_allows(u->role, "valve.close") == (admin || user));
        CHECK(matrix.role_allows(u->role, "security.panic") == admin);
        CHECK(matrix.role_allows(u->role, "system.network.configure") == admin);
        CHECK(matrix.role_allows(u->role, "cloud.configure") == admin);
        CHECK(matrix.role_allows(u->role, "access.manage") == admin);
    }

    CHECK(std::string_view{homeguard::to_string(AuditDecision::DeniedRateLimited)} == "denied_rate_limited");
}
} // namespace

void test_access_control() {
    test_login_and_role_policy();
    test_admin_preservation();
    test_rate_limit_and_capacity();
}
