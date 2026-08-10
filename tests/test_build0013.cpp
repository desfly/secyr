#include "test_framework.hpp"
#include "homeguard/build_info.hpp"
#include "homeguard/controller.hpp"
#include "homeguard/local_api.hpp"
#include <string>

void test_build0013() {
    CHECK(hg::build::number == "0060");
    CHECK(hg::build::version == "0.0.60");
    CHECK(hg::build::label == "HomeGuard-S3 Build-0060");
    const std::string token(48, 'A');
    hg::BearerTokenVerifier verifier(token);
    CHECK(verifier.configured());
    CHECK(verifier.authorized("Bearer " + token));
    CHECK(!verifier.authorized("bearer " + token));
    CHECK(!verifier.authorized("Bearer wrong"));
    CHECK(!verifier.authorized("Bearer " + token + " "));
    verifier.clear();
    CHECK(!verifier.configured());
    CHECK(!verifier.authorized("Bearer " + token));

    CHECK(hg::parse_command_type("arm_home") == hg::CommandType::ArmHome);
    CHECK(hg::parse_command_type("ARM-AWAY") == hg::CommandType::ArmAway);
    CHECK(hg::parse_command_type("open_valves") == hg::CommandType::OpenValves);
    CHECK(!hg::parse_command_type("reboot"));
    CHECK(hg::command_type_name(hg::CommandType::ExitMaintenance) == "exit_maintenance");
    CHECK(hg::command_code_name(hg::CommandCode::ChallengeInvalid) == "challenge_invalid");

    hg::Controller controller;
    controller.health().set(hg::Component::Esp, hg::HealthState::Ok, 10);
    controller.health().set(hg::Component::Nvs, hg::HealthState::Ok, 10);
    controller.update_links({false, true, false}, 2000);
    controller.update_links({false, true, false}, 3001);
    const auto frame = controller.telemetry(5000, 1700000000);
    const auto telemetry = hg::telemetry_json(frame);
    CHECK(telemetry.find("\"sequence\":1") != std::string::npos);
    CHECK(telemetry.find("\"uptimeMs\":5000") != std::string::npos);
    CHECK(telemetry.find("\"mode\":\"disarmed\"") != std::string::npos);
    CHECK(telemetry.find("\"transport\":\"wifi_sta\"") != std::string::npos);
    CHECK(telemetry.find("\"zones\":[") != std::string::npos);
    CHECK(telemetry.find("\"pressures\":[") != std::string::npos);

    const auto health = hg::health_json(controller.health(), controller.transport());
    CHECK(health.find("\"activeTransport\":\"wifi_sta\"") != std::string::npos);
    CHECK(health.find("\"id\":\"esp\"") != std::string::npos);
    CHECK(health.find("\"components\":[") != std::string::npos);

    const auto challenge = controller.issue_challenge(hg::CommandType::OpenValves, 6000, 30000);
    const auto challenge_body = hg::challenge_json(challenge);
    CHECK(challenge_body.find("\"command\":\"open_valves\"") != std::string::npos);
    CHECK(challenge_body.find("\"expiresAtMs\":36000") != std::string::npos);

    const hg::Command command{123456789012345ULL, 6100, hg::CommandType::OpenValves, challenge.token, true};
    const auto result = controller.execute(command, 6100);
    CHECK(result.executed);
    const auto result_body = hg::command_result_json(result);
    CHECK(result_body == R"({"accepted":true,"duplicate":false,"code":"accepted"})");
    const auto duplicate = controller.execute(command, 6200);
    CHECK(duplicate.duplicate);
    CHECK(hg::command_result_json(duplicate).find("\"duplicate\":true") != std::string::npos);
}
