#include "hg_factory_reset.hpp"

#include "hg_commissioning_nvs.hpp"

#include "esp_wifi.h"
#include "nvs.h"

namespace homeguard::idf {
namespace {

esp_err_t erase_namespace(const char* name) {
    nvs_handle_t handle{};
    auto error = nvs_open(name, NVS_READWRITE, &handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (error != ESP_OK) return error;

    error = nvs_erase_all(handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) error = ESP_OK;
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

esp_err_t restore_wifi_defaults() {
    // esp_wifi_restore() requires the Wi-Fi driver to be initialized. Triple-RST
    // factory reset runs before the normal NetworkHttp runtime initializes Wi-Fi,
    // so use a short-lived driver instance here to clear ESP-IDF's persisted
    // esp_wifi_set_config()/mode/protocol state as part of the full reset.
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    auto error = esp_wifi_init(&init);
    if (error != ESP_OK) return error;

    const auto restore_error = esp_wifi_restore();
    const auto deinit_error = esp_wifi_deinit();
    if (restore_error != ESP_OK) return restore_error;
    return deinit_error;
}

}  // namespace

FactoryResetReport FactoryResetManager::erase_mutable_state() const {
    FactoryResetReport report{};

    report.access = erase_namespace("hg_access");

    report.wifi = erase_namespace("hg_wifi");
    if (report.wifi == ESP_OK) {
        report.wifi = restore_wifi_defaults();
    }

    report.cloud = erase_namespace("hg_cloud");
    report.controller_config = erase_namespace("hg-config");
    report.provisioning = erase_namespace("hg-provision");

    // Preserve immutable hardware verification/identity and erase only the
    // user-owned commissioning progress.
    report.commissioning = CommissioningNvsStore{}.erase_commissioning_state();

    return report;
}

}  // namespace homeguard::idf
