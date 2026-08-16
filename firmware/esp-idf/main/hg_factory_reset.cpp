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

}  // namespace

FactoryResetReport FactoryResetManager::erase_mutable_state() const {
    FactoryResetReport report{};

    // Access users/Admin state.
    report.access = erase_namespace("hg_access");

    // HomeGuard Wi-Fi credentials plus any legacy ESP-IDF driver persistence
    // left by builds that used the default WIFI_STORAGE_FLASH mode.
    report.wifi = erase_namespace("hg_wifi");
    if (report.wifi == ESP_OK) {
        report.wifi = esp_wifi_restore();
    }

    // Cloud broker credentials/session configuration.
    report.cloud = erase_namespace("hg_cloud");

    // User-editable controller configuration.
    report.controller_config = erase_namespace("hg-config");

    // Provisioning payload (Wi-Fi/cloud/API token/owner label). The immutable
    // factory identity lives separately in hg-factory and is intentionally kept.
    report.provisioning = erase_namespace("hg-provision");

    // Preserve hardware_v1 identity/verification; reset only mutable
    // commissioning progress/state.
    report.commissioning = CommissioningNvsStore{}.erase_commissioning_state();

    return report;
}

}  // namespace homeguard::idf
