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

    report.access = erase_namespace("hg_access");

    // Triple-RST factory reset runs before the Wi-Fi driver is initialized.
    // HomeGuard owns its credentials in hg_wifi, so erasing that namespace is
    // the authoritative reset step. esp_wifi_restore() is best-effort here:
    // before esp_wifi_init() it legitimately fails, and that must never abort
    // the factory reset or prevent the clean reboot into setup AP mode.
    report.wifi = erase_namespace("hg_wifi");
    if (report.wifi == ESP_OK) {
        (void)esp_wifi_restore();
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
