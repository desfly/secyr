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
#include "hg_commissioning_nvs.hpp"
#include "homeguard/access_control.hpp"
#include "homeguard/boot_readiness.hpp"
#include "homeguard/build_info.hpp"
#include "homeguard/physical_output_runtime.hpp"
#include "homeguard/system_model.hpp"
#include "esp_check.h"

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "nvs_flash.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace {

constexpr const char* kTag = "homeguard_main";

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
homeguard::idf::CommissioningNvsStore g_commissioning_store;
homeguard::AccessControl g_access_control;
hg::HardwareVerificationRecord g_hardware_verification;
hg::CommissioningPersistentState g_commissioning_state;
hg::BootReadinessReport g_boot_readiness;
hg::PhysicalOutputRuntime g_physical_outputs;
hg::SystemEventBus g_system_bus;
hg::SystemModel g_system_model{g_system_bus};
httpd_handle_t g_http_server = nullptr;

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
    std::string credential;
    if (!extract_json_string(body, "request_id", request_id) || request_id.empty() || request_id.size() > 64 ||
        !extract_json_string(body, "command", command) || command.empty() || command.size() > 48) {
        publish_cloud_result(request_id, false, "invalid_command");
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

    if (!extract_json_string(body, "actor", actor) || actor.empty() || actor.size() > 32 ||
        !extract_json_string(body, "credential", credential) || credential.empty() || credential.size() > 32) {
        publish_cloud_result(request_id, false, "credentials_required");
        return;
    }

    const auto decision = g_access_control.authorize(actor, credential, command);
    if (decision != homeguard::AuditDecision::Allowed) {
        publish_cloud_result(request_id, false, homeguard::to_string(decision));
        return;
    }

    const auto now_ms = static_cast<std::uint64_t>(esp_timer_get_time() / 1000);
    if (command == "security.arm_home") {
        const bool changed = g_system_model.set_partition_arm(1, hg::PartitionArmState::Stay, now_ms);
        publish_cloud_result(request_id, changed, changed ? "armed_home" : "arm_failed");
        return;
    }
    if (command == "security.arm_away") {
        const bool changed = g_system_model.set_partition_arm(1, hg::PartitionArmState::Away, now_ms);
        publish_cloud_result(request_id, changed, changed ? "armed_away" : "arm_failed");
        return;
    }
    if (command == "security.disarm") {
        publish_cloud_result(request_id, false, "challenge_required");
        return;
    }

    publish_cloud_result(request_id, false, "unsupported_command");
}

esp_err_t initialize_nvs()
{
    auto error = nvs_flash_init();
    if (error == ESP_ERR_NVS_NO_FREE_PAGES ||
        error == ESP_ERR_NVS_NEW_VERSION_FOUND) {
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
    ESP_LOGI(kTag, "Restored %u access user(s) from NVS",
             static_cast<unsigned>(g_access_control.user_count()));
}

void restore_commissioning_state()
{
    const auto hardware_error = g_commissioning_store.load_hardware(g_hardware_verification);
    const auto commissioning_error = g_commissioning_store.load_commissioning(g_commissioning_state);

    const auto* hardware = hardware_error == ESP_OK ? &g_hardware_verification : nullptr;
    const auto* commissioning = commissioning_error == ESP_OK ? &g_commissioning_state : nullptr;
    g_boot_readiness = hg::evaluate_boot_readiness({hardware, commissioning});

    if (hardware_error != ESP_OK) {
        ESP_LOGW(kTag, "Hardware verification unavailable/rejected (%s)", esp_err_to_name(hardware_error));
    }
    if (commissioning_error != ESP_OK) {
        ESP_LOGW(kTag, "Commissioning state unavailable/rejected (%s)", esp_err_to_name(commissioning_error));
    }

    if (!g_boot_readiness.outputs_allowed()) {
        ESP_LOGW(kTag, "Physical outputs remain FAIL-CLOSED after boot: %s",
                 hg::to_string(g_boot_readiness.status));
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
        ESP_LOGW(kTag, "Persisted WiFi credentials rejected (%s); SoftAP fallback remains active",
                 esp_err_to_name(error));
        return;
    }
    const auto connect_error = g_wifi_provisioning.connect_station(
        credentials.ssid.data(), credentials.password.data());
    if (connect_error != ESP_OK) {
        ESP_LOGW(kTag, "Persisted WiFi STA connect start failed (%s); SoftAP fallback remains active",
                 esp_err_to_name(connect_error));
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

    const auto start_error = g_cloud_link.start(
        config.broker_uri.data(), config.username.data(), config.password.data());
    if (start_error != ESP_OK) {
        ESP_LOGW(kTag, "Cloud link start failed: %s", esp_err_to_name(start_error));
    }
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
        ESP_LOGW(kTag, "Physical outputs unavailable; runtime remains fail-closed (%s)",
                 hg::to_string(g_physical_outputs.state().status));
        return;
    }
    ESP_LOGI(kTag, "Physical output runtime initialized: %s",
             hg::to_string(g_physical_outputs.state().status));
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
    ESP_RETURN_ON_ERROR(
        g_output_http.register_handlers(
            g_http_server, &g_system_model, &g_boot_readiness, &g_physical_outputs, &g_system_bus),
        kTag,
        "output routes");
    ESP_RETURN_ON_ERROR(
        g_service_http.register_handlers(
            g_http_server,
            &g_commissioning_store,
            &g_hardware_verification,
            &g_commissioning_state,
            &g_boot_readiness,
            &g_system_bus),
        kTag,
        "service routes");
    ESP_RETURN_ON_ERROR(
        g_wifi_http.register_handlers(g_http_server, &g_wifi_credentials_store, &g_wifi_provisioning),
        kTag,
        "wifi provisioning routes");
    ESP_RETURN_ON_ERROR(
        g_cloud_http.register_handlers(g_http_server, &g_cloud_config_store, &g_cloud_link),
        kTag,
        "cloud routes");
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
    if (http_error != ESP_OK) {
        ESP_LOGE(kTag, "HTTP server failed: %s", esp_err_to_name(http_error));
    }

    const auto telemetry_error = g_telemetry.start(&g_hardware);
    if (telemetry_error != ESP_OK) {
        ESP_LOGE(kTag, "Telemetry task failed: %s", esp_err_to_name(telemetry_error));
    }

    ESP_LOGI(kTag, "%.*s runtime started",
             static_cast<int>(hg::build::label.size()), hg::build::label.data());
}
