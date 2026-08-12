#include "test_framework.hpp"
#include "homeguard/access_control.hpp"

#include <array>
#include <cstdint>

void test_build0057() {
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

    // The only enabled administrator cannot be disabled or demoted.
    CHECK(!access.would_preserve_admin_access("admin1", AccessRole::Admin, false));
    CHECK(!access.would_preserve_admin_access("admin1", AccessRole::User, true));
    CHECK(!access.would_preserve_admin_access("admin1", AccessRole::Guest, true));
    CHECK(access.would_preserve_admin_access("admin1", AccessRole::Admin, true));

    // Adding ordinary users is fine while at least one enabled Admin remains.
    CHECK(access.would_preserve_admin_access("operator", AccessRole::User, true));
    CHECK(access.set_user("operator", "Operator", AccessRole::User, "2345", salt2, true));
    CHECK(access.enabled_admin_count() == 1U);

    // A second enabled Admin allows the first one to be demoted/disabled.
    CHECK(access.would_preserve_admin_access("admin2", AccessRole::Admin, true));
    CHECK(access.set_user("admin2", "Backup Admin", AccessRole::Admin, "3456", salt3, true));
    CHECK(access.enabled_admin_count() == 2U);
    CHECK(access.would_preserve_admin_access("admin1", AccessRole::User, true));
    CHECK(access.set_user("admin1", "Primary Admin", AccessRole::User, "1234", salt1, true));
    CHECK(access.enabled_admin_count() == 1U);

    // Now admin2 became the last enabled administrator and is protected.
    CHECK(!access.would_preserve_admin_access("admin2", AccessRole::Admin, false));
    CHECK(!access.would_preserve_admin_access("admin2", AccessRole::Guest, true));

    // Re-promoting admin1 gives us two administrators again.
    CHECK(access.would_preserve_admin_access("admin1", AccessRole::Admin, true));
    CHECK(access.set_user("admin1", "Primary Admin", AccessRole::Admin, "1234", salt1, true));
    CHECK(access.enabled_admin_count() == 2U);
    CHECK(access.would_preserve_admin_access("admin2", AccessRole::Admin, false));
}
