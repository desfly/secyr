#include "hg_commissioning_nvs.hpp"

#include "nvs.h"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace homeguard::idf {
namespace {
constexpr const char* kNamespace = "hg_commission";
constexpr const char* kHardwareKey = "hardware_v1";
constexpr const char* kCommissioningKey = "state_v1";
constexpr const char* kLocalApiTokenKey = "api_token_v1";
constexpr std::size_t kLocalApiTokenMin = 32U;
constexpr std::size_t kLocalApiTokenMax = 128U;

bool valid_local_api_token(std::string_view token) {
    if (token.size() < kLocalApiTokenMin || token.size() > kLocalApiTokenMax) return false;
    return std::all_of(token.begin(), token.end(), [](unsigned char ch) {
        return ch >= 0x20U && ch <= 0x7eU;
    });
}

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

esp_err_t CommissioningNvsStore::load_local_api_token(std::string& token) const {
    token.clear();
    nvs_handle_t handle{};
    const auto open_error = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (open_error != ESP_OK) return open_error;

    std::size_t size = 0;
    auto error = nvs_get_str(handle, kLocalApiTokenKey, nullptr, &size);
    if (error != ESP_OK) {
        nvs_close(handle);
        return error;
    }
    if (size <= 1U || size > kLocalApiTokenMax + 1U) {
        nvs_close(handle);
        return ESP_ERR_INVALID_SIZE;
    }

    std::vector<char> buffer(size, '\0');
    error = nvs_get_str(handle, kLocalApiTokenKey, buffer.data(), &size);
    nvs_close(handle);
    if (error != ESP_OK) return error;

    token.assign(buffer.data());
    if (!valid_local_api_token(token)) {
        std::fill(token.begin(), token.end(), '\0');
        token.clear();
        return ESP_ERR_INVALID_STATE;
    }
    return ESP_OK;
}

esp_err_t CommissioningNvsStore::save_local_api_token(std::string_view token) const {
    if (!valid_local_api_token(token)) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    std::string value(token);
    error = nvs_set_str(handle, kLocalApiTokenKey, value.c_str());
    if (error == ESP_OK) error = nvs_commit(handle);
    std::fill(value.begin(), value.end(), '\0');
    nvs_close(handle);
    return error;
}

esp_err_t CommissioningNvsStore::erase_all() const {
    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;

    auto erase_one = [handle](const char* key) {
        auto result = nvs_erase_key(handle, key);
        return result == ESP_ERR_NVS_NOT_FOUND ? ESP_OK : result;
    };

    error = erase_one(kHardwareKey);
    if (error == ESP_OK) error = erase_one(kCommissioningKey);
    if (error == ESP_OK) error = erase_one(kLocalApiTokenKey);
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

}  // namespace homeguard::idf
