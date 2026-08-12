#include "homeguard/user_output_access.hpp"
#include "homeguard/user_zone_access.hpp"
#include "test_framework.hpp"

void test_access_matrix()
{
    homeguard::UserZoneAccess zones;
    CHECK(zones.user_count() == 0);
    CHECK(zones.ensure_user("admin"));
    CHECK(zones.ensure_user("admin"));
    CHECK(zones.user_count() == 1);
    CHECK(!zones.ensure_user(""));
    CHECK(!zones.set_rule("admin", 0, {}));
    CHECK(!zones.set_rule("admin", 17, {}));

    homeguard::ZoneAccessRule z1{};
    z1.visible = true;
    z1.can_arm = true;
    z1.can_disarm = true;
    z1.can_bypass = false;
    CHECK(zones.set_rule("admin", 1, z1));
    const auto* admin_zone = zones.rule("admin", 1);
    CHECK(admin_zone != nullptr);
    CHECK(admin_zone->visible);
    CHECK(admin_zone->can_arm);
    CHECK(admin_zone->can_disarm);
    CHECK(!admin_zone->can_bypass);
    CHECK(zones.rule("unknown", 1) == nullptr);

    homeguard::ZoneAccessRule guest_zone{};
    guest_zone.visible = true;
    CHECK(zones.set_rule("guest", 1, guest_zone));
    const auto* guest_rule = zones.rule("guest", 1);
    CHECK(guest_rule != nullptr);
    CHECK(guest_rule->visible);
    CHECK(!guest_rule->can_arm);
    CHECK(!guest_rule->can_disarm);
    CHECK(!guest_rule->can_bypass);
    CHECK(zones.remove_user("guest"));
    CHECK(zones.rule("guest", 1) == nullptr);

    homeguard::UserOutputAccess outputs;
    CHECK(outputs.ensure_user("user"));
    homeguard::OutputAccessRule valve{};
    valve.visible = true;
    valve.can_on = true;
    valve.can_off = true;
    CHECK(outputs.set_rule("user", 2, valve));
    const auto* user_output = outputs.rule("user", 2);
    CHECK(user_output != nullptr);
    CHECK(user_output->visible);
    CHECK(user_output->can_on);
    CHECK(user_output->can_off);
    CHECK(!outputs.set_rule("user", 0, valve));
    CHECK(!outputs.set_rule("user", 17, valve));

    homeguard::OutputAccessRule guest_output{};
    guest_output.visible = true;
    CHECK(outputs.set_rule("guest", 2, guest_output));
    const auto* guest_output_rule = outputs.rule("guest", 2);
    CHECK(guest_output_rule != nullptr);
    CHECK(guest_output_rule->visible);
    CHECK(!guest_output_rule->can_on);
    CHECK(!guest_output_rule->can_off);
    CHECK(outputs.remove_user("guest"));
    CHECK(outputs.rule("guest", 2) == nullptr);
}
