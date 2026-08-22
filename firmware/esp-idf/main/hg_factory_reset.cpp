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

FactoryResetReport erase_settings_namespaces() {
    FactoryResetReport report{};

    // HomeGuard credentials in hg_wifi are authoritative. The reset runs
    // during early boot before Wi-Fi init; esp_wifi_restore() is best-effort.
    report.wifi = erase_namespace("hg_wifi");
    if (report.wifi == ESP_OK) {
        (void)esp_wifi_restore();
    }

    report.cloud = erase_namespace("hg_cloud");
    report.controller_config = erase_namespace("hg-config");
    report.provisioning = erase_namespace("hg-provision");

    // Preserve immutable hardware verification/factory identity; erase only
    // user-owned commissioning progress.
    report.commissioning = CommissioningNvsStore{}.erase_commissioning_state();
    return report;
}

}  // namespace

FactoryResetReport FactoryResetManager::erase_settings_state() const {
    // access remains ESP_OK and hg_access is deliberately untouched.
    return erase_settings_namespaces();
}

FactoryResetReport FactoryResetManager::erase_mutable_state() const {
    auto report = erase_settings_namespaces();
    report.access = erase_namespace("hg_access");
    return report;
}

}  // namespace homeguard::idf
