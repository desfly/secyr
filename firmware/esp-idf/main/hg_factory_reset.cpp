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
    // Erasing HomeGuard's persisted credentials is therefore the authoritative
    // reset step here. esp_wifi_restore() may legitimately report
    // ESP_ERR_WIFI_NOT_INIT at this stage; that must not abort the reset or
    // prevent the clean reboot into setup AP mode.
    report.wifi = erase_namespace("hg_wifi");
    if (report.wifi == ESP_OK) {
        const auto restore_error = esp_wifi_restore();
        report.wifi = restore_error == ESP_ERR_WIFI_NOT_INIT ? ESP_OK : restore_error;
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
