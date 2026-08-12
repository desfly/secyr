#include "homeguard/access_matrix_policy.hpp"
#include "homeguard/user_output_access.hpp"
#include "homeguard/user_zone_access.hpp"
#include "test_framework.hpp"

#include <array>

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

    hg::SystemEventBus bus;
    hg::SystemModel model{bus};
    CHECK(model.add_zone(1, "Front", hg::ModelZoneType::Perimeter));
    CHECK(model.add_zone(2, "Hall", hg::ModelZoneType::Interior));
    CHECK(model.add_output(1, hg::ModelOutputType::Siren));
    CHECK(model.add_output(2, hg::ModelOutputType::Valve));

    homeguard::AccessControl access;
    const std::array<std::uint8_t, 16> salt{};
    CHECK(access.set_user("admin2", "Admin", homeguard::AccessRole::Admin, "1111", salt, true));
    CHECK(access.set_user("user2", "User", homeguard::AccessRole::User, "2222", salt, true));
    CHECK(access.set_user("guest2", "Guest", homeguard::AccessRole::Guest, "3333", salt, true));

    homeguard::UserZoneAccess default_zones;
    homeguard::UserOutputAccess default_outputs;
    CHECK(homeguard::sync_default_access_matrices(access, model, default_zones, default_outputs));

    const auto* az = default_zones.rule("admin2", 1);
    CHECK(az != nullptr && az->visible && az->can_arm && az->can_disarm && az->can_bypass);
    const auto* uz = default_zones.rule("user2", 1);
    CHECK(uz != nullptr && uz->visible && uz->can_arm && uz->can_disarm && !uz->can_bypass);
    const auto* gz = default_zones.rule("guest2", 1);
    CHECK(gz != nullptr && gz->visible && !gz->can_arm && !gz->can_disarm && !gz->can_bypass);

    const auto* admin_siren = default_outputs.rule("admin2", 1);
    CHECK(admin_siren != nullptr && admin_siren->visible && admin_siren->can_on && admin_siren->can_off);
    const auto* user_siren = default_outputs.rule("user2", 1);
    CHECK(user_siren != nullptr && user_siren->visible && !user_siren->can_on && !user_siren->can_off);
    const auto* user_valve = default_outputs.rule("user2", 2);
    CHECK(user_valve != nullptr && user_valve->visible && user_valve->can_on && user_valve->can_off);
    const auto* guest_valve = default_outputs.rule("guest2", 2);
    CHECK(guest_valve != nullptr && guest_valve->visible && !guest_valve->can_on && !guest_valve->can_off);
}
