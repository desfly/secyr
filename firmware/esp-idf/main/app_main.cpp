#include "hg_hardware_bootstrap.hpp"
#include "hg_build_http.hpp"
#include "hg_infrastructure_http.hpp"
#include "hg_system_http.hpp"
#include "hg_service_http.hpp"
#include "hg_output_http.hpp"
#include "hg_gpio_output_backend.hpp"
#include "hg_telemetry_runtime.hpp"
#include "hg_wifi_provisioning.hpp"
#include "hg_wifi_credentials.hpp"
#include "hg_wifi_http.hpp"
#include "hg_cloud_link.hpp"
#include "hg_cloud_config.hpp"
#include "hg_cloud_http.hpp"
#include "hg_access_nvs.hpp"
#include "hg_access_bootstrap_http.hpp"
#include "hg_access_admin_http.hpp"
#include "hg_commissioning_nvs.hpp"
#include "homeguard/access_control.hpp"
#include "homeguard/admin_user_commands.hpp"
#include "homeguard/self_profile.hpp"
#include "homeguard/boot_readiness.hpp"
#include "homeguard/build_info.hpp"
#include "homeguard/physical_output_runtime.hpp"
#include "homeguard/system_model.hpp"
#include "esp_check.h"

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace {

constexpr const char* kTag = "homeguard_main";
constexpr std::uint64_t kCloudChallengeLifetimeMs = 30000U;

homeguard::idf::HardwareBootstrap g_hardware;
homeguard::idf::TelemetryRuntime g_telemetry;
homeguard::idf::InfrastructureHttp g_http_api;
homeguard::idf::BuildHttp g_build_http;
homeguard::idf::SystemHttp g_system_http;
homeguard::idf::ServiceHttp g_service_http;
homeguard::idf::OutputHttp g_output_http;
homeguard::idf::GpioOutputBackend g_gpio_outputs;
homeguard::idf::WifiProvisioningRuntime g_wifi_provisioning;
homeguard::idf::WifiCredentialStore g_wifi_credentials_store;
homeguard::idf::WifiProvisioningHttp g_wifi_http;
homeguard::idf::CloudLink g_cloud_link;
homeguard::idf::CloudConfigStore g_cloud_config_store;
homeguard::idf::CloudHttp g_cloud_http;
homeguard::idf::AccessNvsStore g_access_store;
homeguard::idf::AccessBootstrapHttp g_access_bootstrap_http;
homeguard::idf::AccessAdminHttp g_access_admin_http;
homeguard::idf::CommissioningNvsStore g_commissioning_store;
homeguard::AccessControl g_access_control;
hg::HardwareVerificationRecord g_hardware_verification;
hg::CommissioningPersistentState g_commissioning_state;
hg::BootReadinessReport g_boot_readiness;
hg::PhysicalOutputRuntime g_physical_outputs;
hg::SystemEventBus g_system_bus;
hg::SystemModel g_system_model{g_system_bus};
httpd_handle_t g_http_server = nullptr;

struct CloudAuthChallenge {
    bool active{false};
    std::string actor;
    std::string command;
    std::string request_id;
    std::string nonce;
    std::uint64_t expires_at_ms{0};
};

CloudAuthChallenge g_cloud_challenge;

std::uint64_t now_ms()
{
    return static_cast<std::uint64_t>(esp_timer_get_time() / 1000);
}

bool extract_json_string(const std::string& body, const char* key, std::string& value)
{
    const std::string token = std::string{"\""} + key + "\"";
    const auto key_pos = body.find(token);
    if (key_pos == std::string::npos) return false;
    const auto colon = body.find(':', key_pos + token.size());
    if (colon == std::string::npos) return false;
    const auto first_quote = body.find('"', colon + 1);
    if (first_quote == std::string::npos) return false;
    const auto second_quote = body.find('"', first_quote + 1);
    if (second_quote == std::string::npos) return false;
    value = body.substr(first_quote + 1, second_quote - first_quote - 1);
    return true;
}

std::string json_escape(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        if (ch == '"' || ch == '\\') escaped.push_back('\\');
        escaped.push_back(ch);
    }
    return escaped;
}

template <std::size_t N>
std::string bytes_hex(const std::array<std::uint8_t, N>& bytes)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(N * 2U);
    for (const auto byte : bytes) {
        out.push_back(digits[(byte >> 4U) & 0x0fU]);
        out.push_back(digits[byte & 0x0fU]);
    }
    return out;
}

const char* arm_state_name(hg::PartitionArmState state)
{
    switch (state) {
        case hg::PartitionArmState::Disarmed: return "disarmed";
        case hg::PartitionArmState::Stay: return "armed_home";
        case hg::PartitionArmState::Away: return "armed_away";
        case hg::PartitionArmState::Alarm: return "alarm";
    }
    return "unknown";
}

const char* sensor_type_name(hg::ModelSensorType type)
{
    switch (type) {
        case hg::ModelSensorType::Digital: return "digital";
        case hg::ModelSensorType::Temperature: return "temperature";
        case hg::ModelSensorType::Pressure: return "pressure";
        case hg::ModelSensorType::Current: return "current";
        case hg::ModelSensorType::Voltage: return "voltage";
    }
    return "unknown";
}

void publish_cloud_result(const std::string& request_id,
                          bool ok,
                          const char* code,
                          const char* extra = nullptr)
{
    std::string response = "{\"request_id\":\"" + json_escape(request_id) + "\",\"ok\":";
    response += ok ? "true" : "false";
    response += ",\"code\":\"";
    response += code;
    response += '"';
    if (extra != nullptr && extra[0] != '\0') {
        response += ',';
        response += extra;
    }
    response += '}';
    (void)g_cloud_link.publish_command_response(response.c_str());
}

void publish_self_profile(const std::string& request_id, const std::string& actor)
{
    homeguard::SelfProfile profile{};
    if (!homeguard::build_self_profile(g_access_control, actor, profile)) {
        publish_cloud_result(request_id, false, "profile_unavailable");
        return;
    }
    std::string extra = "\"id\":\"" + json_escape(profile.id.data()) + "\",\"name\":\"";
    extra += json_escape(profile.name.data());
    extra += "\",\"role\":\"";
    extra += homeguard::to_string(profile.role);
    extra += "\",\"enabled\":";
    extra += profile.enabled ? "true" : "false";
    extra += ",\"can_arm\":";
    extra += profile.can_arm ? "true" : "false";
    extra += ",\"can_disarm\":";
    extra += profile.can_disarm ? "true" : "false";
    extra += ",\"can_control_valves\":";
    extra += profile.can_control_valves ? "true" : "false";
    extra += ",\"can_manage_users\":";
    extra += profile.can_manage_users ? "true" : "false";
    publish_cloud_result(request_id, true, "self_profile", extra.c_str());
}

void publish_sensor_status(const std::string& request_id)
{
    std::string extra = "\"sensors\":[";
    for (std::size_t i = 0; i < g_system_model.sensor_count(); ++i) {
        const auto* sensor = g_system_model.sensor_at(i);
        if (sensor == nullptr) continue;
        if (i != 0U) extra += ',';
        extra += "{\"id\":" + std::to_string(sensor->id);
        extra += ",\"type\":\"";
        extra += sensor_type_name(sensor->type);
        extra += "\",\"online\":";
        extra += sensor->online ? "true" : "false";
        extra += ",\"battery_percent\":" + std::to_string(sensor->battery_percent);
        extra += ",\"rssi_dbm\":" + std::to_string(sensor->rssi_dbm);
        extra += ",\"last_seen_ms\":" + std::to_string(sensor->last_seen_ms);
        extra += '}';
    }
    extra += ']';
    publish_cloud_result(request_id, true, "sensors", extra.c_str());
}

void publish_admin_directory(const std::string& request_id,
                             const homeguard::AdminUserCommandResult& result)
{
    if (result.status != homeguard::AdminUserCommandStatus::Ok) {
        publish_cloud_result(request_id, false, homeguard::to_string(result.status));
        return;
    }
    std::string extra = "\"users\":[";
    for (std::size_t i = 0; i < result.directory.count; ++i) {
        if (i != 0U) extra += ',';
        const auto& user = result.directory.users[i];
        extra += "{\"id\":\"" + json_escape(user.id.data()) + "\",\"name\":\"";
        extra += json_escape(user.name.data());
        extra += "\",\"role\":\"";
        extra += homeguard::to_string(user.role);
        extra += "\",\"enabled\":";
        extra += user.enabled ? "true" : "false";
        extra += '}';
    }
    extra += ']';
    publish_cloud_result(request_id, true, "users", extra.c_str());
}

void issue_cloud_challenge(const std::string& request_id,
                           const std::string& actor,
                           const std::string& target_command)
{
    const auto* user = g_access_control.find_user(actor);
    if (user == nullptr || !user->enabled) {
        publish_cloud_result(request_id, false, "denied_unknown_user");
        return;
    }
    if (!g_access_control.role_allows(user->role, target_command)) {
        publish_cloud_result(request_id, false, "denied_role");
        return;
    }
    std::array<std::uint8_t, 16> random_bytes{};
    esp_fill_random(random_bytes.data(), random_bytes.size());
    g_cloud_challenge.active = true;
    g_cloud_challenge.actor = actor;
    g_cloud_challenge.command = target_command;
    g_cloud_challenge.request_id = request_id;
    g_cloud_challenge.nonce = bytes_hex(random_bytes);
    g_cloud_challenge.expires_at_ms = now_ms() + kCloudChallengeLifetimeMs;
    std::string extra = "\"nonce\":\"" + g_cloud_challenge.nonce + "\",\"salt\":\"";
    extra += bytes_hex(user->salt);
    extra += "\",\"expires_in_ms\":" + std::to_string(kCloudChallengeLifetimeMs);
    publish_cloud_result(request_id, true, "auth_challenge", extra.c_str());
}

bool authorize_cloud_command(const std::string& body,
                             const std::string& request_id,
                             const std::string& command,
                             const std::string& actor)
{
    std::string proof;
    if (!extract_json_string(body, "auth_proof", proof) || proof.size() != 64U) {
        publish_cloud_result(request_id, false, "auth_proof_required");
        return false;
    }
    const auto timestamp = now_ms();
    if (!g_cloud_challenge.active || timestamp > g_cloud_challenge.expires_at_ms) {
        g_cloud_challenge.active = false;
        publish_cloud_result(request_id, false, "challenge_expired");
        return false;
    }
    if (g_cloud_challenge.actor != actor ||
        g_cloud_challenge.command != command ||
        g_cloud_challenge.request_id != request_id) {
        g_cloud_challenge.active = false;
        publish_cloud_result(request_id, false, "challenge_mismatch");
        return false;
    }
    const std::string nonce = g_cloud_challenge.nonce;
    g_cloud_challenge.active = false;
    const auto decision = g_access_control.authorize_cloud_proof(actor, command, nonce, request_id, proof);
    if (decision != homeguard::AuditDecision::Allowed) {
        publish_cloud_result(request_id, false, homeguard::to_string(decision));
        return false;
    }
    return true;
}

bool persist_admin_change(const homeguard::AccessControl& backup,
                          const std::string& request_id,
                          const homeguard::AdminUserCommandResult& result)
{
    if (result.status != homeguard::AdminUserCommandStatus::Ok) {
        publish_admin_directory(request_id, result);
        return false;
    }
    const auto save_error = g_access_store.save(g_access_control);
    if (save_error != ESP_OK) {
        g_access_control = backup;
        ESP_LOGE(kTag, "Cloud access mutation NVS save failed: %s", esp_err_to_name(save_error));
        publish_cloud_result(request_id, false, "nvs_save_failed");
        return false;
    }
    publish_admin_directory(request_id, result);
    return true;
}

bool set_cloud_valve(bool active, std::uint64_t timestamp)
{
    if (!g_boot_readiness.outputs_allowed()) return false;
    const auto* output = g_system_model.output(2);
    if (output == nullptr || output->type != hg::ModelOutputType::Valve) return false;
    const bool previous = output->active;
    if (!g_system_model.set_output_active(2, active, timestamp)) return false;
    if (g_physical_outputs.synchronize(g_system_model, g_boot_readiness)) return true;
    (void)g_system_model.set_output_active(2, previous, timestamp);
    (void)g_physical_outputs.synchronize(g_system_model, g_boot_readiness);
    return false;
}

void handle_cloud_command(const char* payload, std::size_t length, void*)
{
    if (payload == nullptr || length == 0 || length > 1024) {
        publish_cloud_result("", false, "invalid_payload");
        return;
    }
    const std::string body(payload, length);
    std::string request_id;
    std::string command;
    std::string actor;
    if (!extract_json_string(body, "request_id", request_id) || request_id.empty() || request_id.size() > 64 ||
        !extract_json_string(body, "command", command) || command.empty() || command.size() > 48) {
        publish_cloud_result(request_id, false, "invalid_command");
        return;
    }
    if (command == "auth.challenge") {
        std::string target_command;
        if (!extract_json_string(body, "actor", actor) || actor.empty() || actor.size() > 23U ||
            !extract_json_string(body, "target_command", target_command) || target_command.empty() || target_command.size() > 48U) {
            publish_cloud_result(request_id, false, "challenge_fields_required");
            return;
        }
        issue_cloud_challenge(request_id, actor, target_command);
        return;
    }
    if (!extract_json_string(body, "actor", actor) || actor.empty() || actor.size() > 23U) {
        publish_cloud_result(request_id, false, "actor_required");
        return;
    }
    if (!authorize_cloud_command(body, request_id, command, actor)) return;

    if (command == "profile.self") {
        publish_self_profile(request_id, actor);
        return;
    }
    if (command == "sensors.status") {
        publish_sensor_status(request_id);
        return;
    }
    if (command == "system.status") {
        const auto* partition = g_system_model.partition(1);
        if (partition == nullptr) {
            publish_cloud_result(request_id, false, "partition_unavailable");
            return;
        }
        std::string extra = "\"arm_state\":\"";
        extra += arm_state_name(partition->arm_state);
        extra += "\",\"outputs_allowed\":";
        extra += g_boot_readiness.outputs_allowed() ? "true" : "false";
        publish_cloud_result(request_id, true, "status", extra.c_str());
        return;
    }
    if (command == "access.users.list") {
        publish_admin_directory(request_id, homeguard::admin_users_list(g_access_control, actor));
        return;
    }
    if (command == "access.users.enable" || command == "access.users.disable" || command == "access.users.delete") {
        std::string target_id;
        if (!extract_json_string(body, "target_id", target_id) || target_id.empty() || target_id.size() > 23U) {
            publish_cloud_result(request_id, false, "target_id_required");
            return;
        }
        const homeguard::AccessControl backup = g_access_control;
        if (command == "access.users.delete") {
            (void)persist_admin_change(backup, request_id, homeguard::admin_user_delete(g_access_control, actor, target_id));
        } else {
            (void)persist_admin_change(
                backup,
                request_id,
                homeguard::admin_user_set_enabled(g_access_control, actor, target_id, command == "access.users.enable"));
        }
        return;
    }

    const auto timestamp = now_ms();
    if (command == "security.arm_home") {
        const bool changed = g_system_model.set_partition_arm(1, hg::PartitionArmState::Stay, timestamp);
        publish_cloud_result(request_id, changed, changed ? "armed_home" : "arm_failed");
        return;
    }
    if (command == "security.arm_away") {
        const bool changed = g_system_model.set_partition_arm(1, hg::PartitionArmState::Away, timestamp);
        publish_cloud_result(request_id, changed, changed ? "armed_away" : "arm_failed");
        return;
    }
    if (command == "security.disarm") {
        const bool changed = g_system_model.set_partition_arm(1, hg::PartitionArmState::Disarmed, timestamp);
        publish_cloud_result(request_id, changed, changed ? "disarmed" : "disarm_failed");
        return;
    }
    if (command == "valve.open" || command == "valve.close") {
        if (!g_boot_readiness.outputs_allowed()) {
            publish_cloud_result(request_id, false, "outputs_fail_closed");
            return;
        }
        const bool opening = command == "valve.open";
        const bool changed = set_cloud_valve(opening, timestamp);
        publish_cloud_result(request_id, changed,
                             changed ? (opening ? "valve_open" : "valve_closed") : "valve_write_failed");
        return;
    }
    publish_cloud_result(request_id, false, "unsupported_command");
}

esp_err_t initialize_nvs()
{
    auto error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES || error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        error = nvs_flash_init();
    }
    return error;
}

void restore_access_control()
{
    const auto error = g_access_store.load(g_access_control);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(kTag, "No persisted access database; access starts fail-closed with zero users");
        return;
    }
    if (error != ESP_OK) {
        g_access_control.clear_users();
        ESP_LOGE(kTag, "Access database rejected (%s); access starts fail-closed", esp_err_to_name(error));
        return;
    }
    ESP_LOGI(kTag, "Restored %u access user(s) from NVS", static_cast<unsigned>(g_access_control.user_count()));
}

void restore_commissioning_state()
{
    const auto hardware_error = g_commissioning_store.load_hardware(g_hardware_verification);
    const auto commissioning_error = g_commissioning_store.load_commissioning(g_commissioning_state);
    const auto* hardware = hardware_error == ESP_OK ? &g_hardware_verification : nullptr;
    const auto* commissioning = commissioning_error == ESP_OK ? &g_commissioning_state : nullptr;
    g_boot_readiness = hg::evaluate_boot_readiness({hardware, commissioning});
    if (hardware_error != ESP_OK) ESP_LOGW(kTag, "Hardware verification unavailable/rejected (%s)", esp_err_to_name(hardware_error));
    if (commissioning_error != ESP_OK) ESP_LOGW(kTag, "Commissioning state unavailable/rejected (%s)", esp_err_to_name(commissioning_error));
    if (!g_boot_readiness.outputs_allowed()) {
        ESP_LOGW(kTag, "Physical outputs remain FAIL-CLOSED after boot: %s", hg::to_string(g_boot_readiness.status));
    } else {
        ESP_LOGI(kTag, "Verified commissioning state restored; physical output gate is ready");
    }
}

void restore_wifi_credentials()
{
    homeguard::idf::WifiCredentials credentials{};
    const auto error = g_wifi_credentials_store.load(credentials);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(kTag, "No persisted WiFi credentials; SoftAP provisioning remains active");
        return;
    }
    if (error != ESP_OK) {
        ESP_LOGW(kTag, "Persisted WiFi credentials rejected (%s); SoftAP fallback remains active", esp_err_to_name(error));
        return;
    }
    const auto connect_error = g_wifi_provisioning.connect_station(credentials.ssid.data(), credentials.password.data());
    if (connect_error != ESP_OK) {
        ESP_LOGW(kTag, "Persisted WiFi STA connect start failed (%s); SoftAP fallback remains active", esp_err_to_name(connect_error));
    }
}

void restore_cloud_config()
{
    g_cloud_link.set_command_handler(&handle_cloud_command);
    const auto identity_error = g_cloud_link.prepare_identity();
    if (identity_error != ESP_OK) {
        ESP_LOGW(kTag, "Cloud device identity unavailable: %s", esp_err_to_name(identity_error));
    } else {
        ESP_LOGI(kTag, "Cloud device id: %s", g_cloud_link.device_id());
    }
    homeguard::idf::CloudConfig config{};
    const auto error = g_cloud_config_store.load(config);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(kTag, "Cloud not configured; local control remains available");
        return;
    }
    if (error != ESP_OK) {
        ESP_LOGW(kTag, "Persisted cloud config rejected: %s", esp_err_to_name(error));
        return;
    }
    const auto start_error = g_cloud_link.start(config.broker_uri.data(), config.username.data(), config.password.data());
    if (start_error != ESP_OK) ESP_LOGW(kTag, "Cloud link start failed: %s", esp_err_to_name(start_error));
}

void initialize_system_model()
{
    g_system_model.add_partition(1);
    g_system_model.add_zone(1, "Zone 1", hg::ModelZoneType::Perimeter);
    g_system_model.add_zone(2, "Zone 2", hg::ModelZoneType::Interior);
    g_system_model.add_output(1, hg::ModelOutputType::Siren);
    g_system_model.add_output(2, hg::ModelOutputType::Valve);
}

void initialize_physical_outputs()
{
    if (!g_physical_outputs.initialize(g_gpio_outputs, g_hardware_verification, g_boot_readiness)) {
        ESP_LOGW(kTag, "Physical outputs unavailable; runtime remains fail-closed (%s)", hg::to_string(g_physical_outputs.state().status));
        return;
    }
    ESP_LOGI(kTag, "Physical output runtime initialized: %s", hg::to_string(g_physical_outputs.state().status));
}

esp_err_t start_http_server()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 32;
    config.stack_size = 8192;
    config.lru_purge_enable = true;
    ESP_RETURN_ON_ERROR(httpd_start(&g_http_server, &config), kTag, "httpd_start");
    ESP_RETURN_ON_ERROR(g_http_api.register_handlers(g_http_server, &g_hardware), kTag, "hardware routes");
    ESP_RETURN_ON_ERROR(g_system_http.register_handlers(g_http_server, &g_system_model, &g_system_bus), kTag, "system routes");
    ESP_RETURN_ON_ERROR(g_output_http.register_handlers(g_http_server, &g_system_model, &g_boot_readiness, &g_physical_outputs, &g_system_bus), kTag, "output routes");
    ESP_RETURN_ON_ERROR(g_service_http.register_handlers(g_http_server, &g_commissioning_store, &g_hardware_verification, &g_commissioning_state, &g_boot_readiness, &g_system_bus), kTag, "service routes");
    ESP_RETURN_ON_ERROR(g_wifi_http.register_handlers(g_http_server, &g_wifi_credentials_store, &g_wifi_provisioning), kTag, "wifi provisioning routes");
    ESP_RETURN_ON_ERROR(g_cloud_http.register_handlers(g_http_server, &g_cloud_config_store, &g_cloud_link), kTag, "cloud routes");
    ESP_RETURN_ON_ERROR(g_access_bootstrap_http.register_handlers(g_http_server, &g_access_control, &g_access_store), kTag, "access bootstrap routes");
    ESP_RETURN_ON_ERROR(g_access_admin_http.register_handlers(g_http_server, &g_access_control, &g_access_store), kTag, "access admin routes");
    return g_build_http.register_handlers(g_http_server);
}

}  // namespace

extern "C" void app_main()
{
    ESP_ERROR_CHECK(initialize_nvs());
    restore_access_control();
    restore_commissioning_state();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    initialize_system_model();
    const bool provisioning_required = !g_boot_readiness.outputs_allowed();
    const auto wifi_error = g_wifi_provisioning.start(provisioning_required);
    if (wifi_error != ESP_OK) {
        ESP_LOGE(kTag, "First-boot WiFi provisioning failed: %s", esp_err_to_name(wifi_error));
    } else {
        restore_wifi_credentials();
    }
    restore_cloud_config();
    initialize_physical_outputs();
    const auto hardware_error = g_hardware.initialize();
    if (hardware_error != ESP_OK) {
        ESP_LOGE(kTag, "Hardware bootstrap failed: %s", esp_err_to_name(hardware_error));
    } else {
        ESP_LOGI(kTag, "Hardware bootstrap completed");
    }
    const auto http_error = start_http_server();
    if (http_error != ESP_OK) ESP_LOGE(kTag, "HTTP server failed: %s", esp_err_to_name(http_error));
    const auto telemetry_error = g_telemetry.start(&g_hardware, &g_system_model, &g_cloud_link);
    if (telemetry_error != ESP_OK) ESP_LOGE(kTag, "Telemetry task failed: %s", esp_err_to_name(telemetry_error));
    ESP_LOGI(kTag, "%.*s runtime started", static_cast<int>(hg::build::label.size()), hg::build::label.data());
}
