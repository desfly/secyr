#include "hg_factory_reset.hpp"

#include "hg_commissioning_nvs.hpp"

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

    // Access users/Admin bootstrap state.
    report.access = erase_namespace("hg_access");

    // Wi-Fi credentials and network setup.
    report.wifi = erase_namespace("hg_wifi");

    // Cloud broker credentials/session configuration.
    report.cloud = erase_namespace("hg_cloud");

    // Preserve hg_commission/hardware_v1; only user-owned commissioning state is reset.
    report.commissioning = CommissioningNvsStore{}.erase_commissioning_state();

    return report;
}

}  // namespace homeguard::idf
