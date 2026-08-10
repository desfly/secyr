#include "test_framework.hpp"
#include "homeguard/access_control.hpp"
#include "homeguard/device_command_router.hpp"

#include <array>
#include <cstdint>

void test_build0037() {
    homeguard::AccessControl access;
    const std::array<std::uint8_t, 16> admin_salt{1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};
    const std::array<std::uint8_t, 16> user_salt{16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1};
    const std::array<std::uint8_t, 16> guest_salt{9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9};

    CHECK(access.set_user("admin", "Administrator", homeguard::AccessRole::Admin, "246810", admin_salt));
    CHECK(access.set_user("resident", "Resident", homeguard::AccessRole::User, "1357", user_salt));
    CHECK(access.set_user("guest", "Guest", homeguard::AccessRole::Guest, "9999", guest_salt));
    CHECK(access.user_count() == 3U);

    const auto* resident = access.find_user("resident");
    CHECK(resident != nullptr);
    CHECK(access.verify_pin(*resident, "1357"));
    CHECK(!access.verify_pin(*resident, "0000"));
    CHECK(access.role_allows(homeguard::AccessRole::User, "security.disarm"));
    CHECK(access.role_allows(homeguard::AccessRole::User, "valve.open"));
    CHECK(access.role_allows(homeguard::AccessRole::User, "valve.close"));
    CHECK(!access.role_allows(homeguard::AccessRole::Guest, "security.arm_home"));
    CHECK(!access.role_allows(homeguard::AccessRole::Guest, "valve.open"));

    homeguard::DeviceApiState state;
    homeguard::DeviceCommandRouter router(state, &access);

    const auto arm = router.handle({"req-1", "resident", "security.arm_away", "", "", "1357"});
    CHECK(arm.code == homeguard::CommandResultCode::Accepted);
    CHECK(state.security_mode == homeguard::SecurityMode::ArmedAway);

    const auto bad_pin = router.handle({"req-2", "resident", "security.disarm", "", "", "0000"});
    CHECK(bad_pin.code == homeguard::CommandResultCode::Unauthorized);
    CHECK(state.security_mode == homeguard::SecurityMode::ArmedAway);

    const auto user_valve = router.handle({"req-3", "resident", "valve.open", "cold", "", "1357"});
    CHECK(user_valve.code == homeguard::CommandResultCode::Accepted);

    const auto guest = router.handle({"req-4", "guest", "security.disarm", "", "", "9999"});
    CHECK(guest.code == homeguard::CommandResultCode::Unauthorized);

    const auto admin = router.handle({"req-5", "admin", "security.disarm", "", "", "246810"});
    CHECK(admin.code == homeguard::CommandResultCode::Accepted);
    CHECK(state.security_mode == homeguard::SecurityMode::Disarmed);

    CHECK(access.audit_size() == 5U);
    const auto* first = access.audit_at_oldest(0);
    const auto* last = access.audit_at_oldest(4);
    CHECK(first != nullptr && first->decision == homeguard::AuditDecision::Allowed);
    CHECK(last != nullptr && last->decision == homeguard::AuditDecision::Allowed);
    CHECK(access.audit_at_oldest(5) == nullptr);
}
