#include "test_framework.hpp"
#include "homeguard/access_control.hpp"

#include <array>
#include <cstdint>

namespace {
std::uint64_t g_auth_clock_ms = 0;
std::uint64_t fake_auth_clock() { return g_auth_clock_ms; }
}

void test_build0058() {
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

    // Full 8-account capacity and exact role matrix.
    AccessControl matrix;
    for (std::size_t i = 0; i < AccessControl::user_capacity; ++i) {
        std::array<std::uint8_t, 16> salt{};
        salt[0] = static_cast<std::uint8_t>(0x70U + i);
        const auto role = i < 2U ? AccessRole::Admin : (i < 6U ? AccessRole::User : AccessRole::Guest);
        const char* ids[] = {"admin1","admin2","user1","user2","user3","user4","guest1","guest2"};
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
