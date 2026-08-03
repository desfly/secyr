#include "hg_hardware_bootstrap.hpp"
#include "hg_build_http.hpp"
#include "hg_infrastructure_http.hpp"
#include "hg_telemetry_runtime.hpp"
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
homeguard::idf::InfrastructureHttp g_http_api;
homeguard::idf::BuildHttp g_build_http;
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

esp_err_t start_http_server()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 32;
    config.stack_size = 8192;
    config.lru_purge_enable = true;

    ESP_RETURN_ON_ERROR(
        httpd_start(&g_http_server, &config),
        kTag,
        "httpd_start");

    ESP_RETURN_ON_ERROR(
        g_http_api.register_handlers(
            g_http_server,
            &g_hardware),
        kTag,
        "hardware routes");

    return g_build_http.register_handlers(g_http_server);
}

}  // namespace

extern "C" void app_main()
{
    ESP_ERROR_CHECK(initialize_nvs());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    const auto hardware_error = g_hardware.initialize();
    if (hardware_error != ESP_OK) {
        ESP_LOGE(
            kTag,
            "Hardware bootstrap failed: %s",
            esp_err_to_name(hardware_error));
    } else {
        ESP_LOGI(
            kTag,
            "Hardware bootstrap completed");
    }

    const auto http_error = start_http_server();
    if (http_error != ESP_OK) {
        ESP_LOGE(
            kTag,
            "HTTP server failed: %s",
            esp_err_to_name(http_error));
    }

    const auto telemetry_error =
        g_telemetry.start(&g_hardware);
    if (telemetry_error != ESP_OK) {
        ESP_LOGE(
            kTag,
            "Telemetry task failed: %s",
            esp_err_to_name(telemetry_error));
    }

    ESP_LOGI(
        kTag,
        "HomeGuard-S3 Build-0022 runtime started");
}
