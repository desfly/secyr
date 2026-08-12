#include "hg_hardware_bootstrap.hpp"
#include "hg_web_http.hpp"
#include "hg_network_http.hpp"
#include "hg_build_http.hpp"
#include "hg_infrastructure_http.hpp"
#include "hg_system_http.hpp"
#include "hg_service_http.hpp"
#include "hg_output_http.hpp"
#include "hg_gpio_output_backend.hpp"
#include "hg_telemetry_runtime.hpp"
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
#include "nvs_flash.h"

namespace {

constexpr const char* kTag = "homeguard_main";

homeguard::idf::HardwareBootstrap g_hardware;
homeguard::idf::TelemetryRuntime g_telemetry;
homeguard::idf::WebHttp g_web_http;
homeguard::idf::NetworkHttp g_network_http;
homeguard::idf::InfrastructureHttp g_http_api;
homeguard::idf::BuildHttp g_build_http;
homeguard::idf::SystemHttp g_system_http;
homeguard::idf::ServiceHttp g_service_http;
homeguard::idf::OutputHttp g_output_http;
homeguard::idf::GpioOutputBackend g_gpio_outputs;
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

void initialize_system_model()
{
    g_system_model.add_partition(1);
    g_system_model.add_zone(1, "Zone 1", hg::ModelZoneType::Perimeter);
    g_system_model.add_zone(2, "Zone 2", hg::ModelZoneType::Interior);
    g_system_model.add_output(1, hg::ModelOutputType::Siren);
    g_system_model.add_output(2, hg::ModelOutputType::Valve);
    g_system_model.add_output(3, hg::ModelOutputType::Valve);
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
    config.max_uri_handlers = 48;
    config.stack_size = 8192;
    config.lru_purge_enable = true;

    ESP_RETURN_ON_ERROR(httpd_start(&g_http_server, &config), kTag, "httpd_start");
    ESP_RETURN_ON_ERROR(g_web_http.register_handlers(g_http_server), kTag, "web routes");
    ESP_RETURN_ON_ERROR(g_network_http.register_handlers(g_http_server), kTag, "network routes");
    ESP_RETURN_ON_ERROR(g_http_api.register_handlers(g_http_server, &g_hardware), kTag, "hardware routes");
    ESP_RETURN_ON_ERROR(
        g_system_http.register_handlers(
            g_http_server, &g_system_model, &g_system_bus, &g_access_control),
        kTag,
        "system routes");
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

    const auto network_error = g_network_http.begin();
    if (network_error != ESP_OK) {
        ESP_LOGE(kTag, "Wi-Fi network runtime failed: %s", esp_err_to_name(network_error));
    } else {
        ESP_LOGI(kTag, "Wi-Fi network runtime ready");
    }

    initialize_system_model();
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
