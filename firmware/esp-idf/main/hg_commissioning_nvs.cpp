#include "hg_commissioning_nvs.hpp"

#include "nvs.h"

#include <cstddef>

namespace homeguard::idf {
namespace {
constexpr const char* kNamespace = "hg_commission";
constexpr const char* kHardwareKey = "hardware_v1";
constexpr const char* kCommissioningKey = "state_v1";

template <typename T>
esp_err_t load_blob(const char* key, T& value) {
    nvs_handle_t handle{};
    const auto open_error = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (open_error != ESP_OK) return open_error;

    std::size_t size = sizeof(T);
    const auto read_error = nvs_get_blob(handle, key, &value, &size);
    nvs_close(handle);
    if (read_error != ESP_OK) return read_error;
    return size == sizeof(T) ? ESP_OK : ESP_ERR_INVALID_SIZE;
}

template <typename T>
esp_err_t save_blob(const char* key, const T& value) {
    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    error = nvs_set_blob(handle, key, &value, sizeof(T));
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}
}  // namespace

esp_err_t CommissioningNvsStore::load_hardware(hg::HardwareVerificationRecord& record) const {
    const auto error = load_blob(kHardwareKey, record);
    if (error != ESP_OK) return error;
    return hg::validate_hardware_verification(record) == hg::HardwareVerificationStatus::Valid
        ? ESP_OK : ESP_ERR_INVALID_CRC;
}

esp_err_t CommissioningNvsStore::save_hardware(const hg::HardwareVerificationRecord& record) const {
    if (hg::validate_hardware_verification(record) != hg::HardwareVerificationStatus::Valid) {
        return ESP_ERR_INVALID_ARG;
    }
    return save_blob(kHardwareKey, record);
}

esp_err_t CommissioningNvsStore::load_commissioning(hg::CommissioningPersistentState& state) const {
    const auto error = load_blob(kCommissioningKey, state);
    if (error != ESP_OK) return error;
    return hg::validate_commissioning_state(state) == hg::CommissioningStateValidation::Valid
        ? ESP_OK : ESP_ERR_INVALID_STATE;
}

esp_err_t CommissioningNvsStore::save_commissioning(const hg::CommissioningPersistentState& state) const {
    if (hg::validate_commissioning_state(state) != hg::CommissioningStateValidation::Valid) {
        return ESP_ERR_INVALID_ARG;
    }
    return save_blob(kCommissioningKey, state);
}

esp_err_t CommissioningNvsStore::erase_all() const {
    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    error = nvs_erase_all(handle);
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

}  // namespace homeguard::idf
