#include "test_framework.hpp"
#include "homeguard/config_exchange.hpp"

#include <algorithm>
#include <string>
#include <string_view>

namespace {
template <std::size_t N>
void set_text(std::array<char, N>& out, std::string_view value)
{
    out.fill('\0');
    const auto n = std::min(value.size(), out.size() - 1U);
    std::copy_n(value.begin(), n, out.begin());
}

homeguard::HomeGuardConfigDocument valid_document()
{
    homeguard::HomeGuardConfigDocument doc{};
    doc.zone_count = 2;
    doc.zones[0].id = 1;
    set_text(doc.zones[0].name, "Front door");
    doc.zones[0].type = homeguard::ConfigZoneType::EntryExit;
    doc.zones[0].entry_delay_sec = 30;
    doc.zones[0].exit_delay_sec = 45;
    doc.zones[1].id = 2;
    set_text(doc.zones[1].name, "Kitchen flood");
    doc.zones[1].type = homeguard::ConfigZoneType::Flood24h;
    doc.zones[1].entry_delay_sec = 0;
    doc.zones[1].exit_delay_sec = 0;

    doc.output_count = 2;
    doc.outputs[0].id = 1;
    set_text(doc.outputs[0].name, "Siren");
    doc.outputs[0].type = homeguard::ConfigOutputType::Siren;
    doc.outputs[1].id = 2;
    set_text(doc.outputs[1].name, "Water valve");
    doc.outputs[1].type = homeguard::ConfigOutputType::Valve;

    doc.user_count = 3;
    set_text(doc.users[0].id, "admin");
    set_text(doc.users[0].name, "Administrator");
    doc.users[0].role = homeguard::AccessRole::Admin;
    set_text(doc.users[1].id, "user1");
    set_text(doc.users[1].name, "User 1");
    doc.users[1].role = homeguard::AccessRole::User;
    set_text(doc.users[2].id, "guest");
    set_text(doc.users[2].name, "Guest");
    doc.users[2].role = homeguard::AccessRole::Guest;

    (void)doc.zone_access.set_rule("admin", 1, {true, true, true, true});
    (void)doc.zone_access.set_rule("user1", 1, {true, true, true, false});
    (void)doc.zone_access.set_rule("guest", 1, {true, false, false, false});
    (void)doc.output_access.set_rule("admin", 2, {true, true, true});
    (void)doc.output_access.set_rule("user1", 2, {true, true, true});
    (void)doc.output_access.set_rule("guest", 2, {true, false, false});
    return doc;
}
}

void test_build0060_config()
{
    auto doc = valid_document();
    EXPECT_TRUE(homeguard::validate_config_document(doc).ok());

    const std::string json = homeguard::export_config_json(doc);
    EXPECT_TRUE(json.find("\"schema\":\"homeguard-s3-config\"") != std::string::npos);
    EXPECT_TRUE(json.find("\"entryDelaySec\":30") != std::string::npos);
    EXPECT_TRUE(json.find("\"outputAccess\"") != std::string::npos);
    EXPECT_TRUE(json.find("pin") == std::string::npos);

    auto no_admin = doc;
    no_admin.users[0].enabled = false;
    EXPECT_TRUE(homeguard::validate_config_document(no_admin).error == homeguard::ConfigValidationError::MissingEnabledAdmin);

    auto guest_zone_control = doc;
    (void)guest_zone_control.zone_access.set_rule("guest", 1, {true, true, false, false});
    EXPECT_TRUE(homeguard::validate_config_document(guest_zone_control).error == homeguard::ConfigValidationError::GuestControlDenied);

    auto guest_output_control = doc;
    (void)guest_output_control.output_access.set_rule("guest", 2, {true, true, false});
    EXPECT_TRUE(homeguard::validate_config_document(guest_output_control).error == homeguard::ConfigValidationError::GuestControlDenied);

    auto duplicate_zone = doc;
    duplicate_zone.zones[1].id = 1;
    EXPECT_TRUE(homeguard::validate_config_document(duplicate_zone).error == homeguard::ConfigValidationError::DuplicateZoneId);

    auto bad_version = doc;
    bad_version.schema_version = 999;
    EXPECT_TRUE(homeguard::validate_config_document(bad_version).error == homeguard::ConfigValidationError::UnsupportedVersion);
}
