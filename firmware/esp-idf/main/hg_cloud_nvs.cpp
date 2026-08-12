#include "hg_cloud_nvs.hpp"

#include "nvs.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace homeguard::idf {
namespace {
constexpr char kNamespace[] = "hg_cloud";
constexpr char kEnabled[] = "enabled";
constexpr char kBroker[] = "broker";
constexpr char kUsername[] = "username";
constexpr char kPassword[] = "password";

esp_err_t get_string(nvs_handle_t handle, const char* key, std::string& out)
{
    std::size_t size = 0;
    auto error = nvs_get_str(handle, key, nullptr, &size);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        out.clear();
        return ESP_OK;
    }
    if (error != ESP_OK) return error;
    if (size == 0 || size > 257) return ESP_ERR_INVALID_SIZE;
    std::array<char, 257> buffer{};
    error = nvs_get_str(handle, key, buffer.data(), &size);
    if (error != ESP_OK) return error;
    out.assign(buffer.data());
    return ESP_OK;
}
}

esp_err_t CloudNvsStore::load(CloudConfig& config) const
{
    config = {};
    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (error != ESP_OK) return error;

    std::uint8_t enabled = 0;
    error = nvs_get_u8(handle, kEnabled, &enabled);
    if (error == ESP_ERR_NVS_NOT_FOUND) {
        nvs_close(handle);
        return ESP_ERR_NVS_NOT_FOUND;
    }
    if (error == ESP_OK) error = get_string(handle, kBroker, config.broker_uri);
    if (error == ESP_OK) error = get_string(handle, kUsername, config.username);
    if (error == ESP_OK) error = get_string(handle, kPassword, config.password);
    nvs_close(handle);
    if (error != ESP_OK) return error;

    config.enabled = enabled != 0;
    if (config.broker_uri.size() > 256 || config.username.size() > 128 || config.password.size() > 128) {
        config = {};
        return ESP_ERR_INVALID_SIZE;
    }
    return ESP_OK;
}

esp_err_t CloudNvsStore::save(const CloudConfig& config) const
{
    if (config.broker_uri.size() > 256 || config.username.size() > 128 || config.password.size() > 128) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config.enabled && config.broker_uri.empty()) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    if (error == ESP_OK) error = nvs_set_u8(handle, kEnabled, config.enabled ? 1 : 0);
    if (error == ESP_OK) error = nvs_set_str(handle, kBroker, config.broker_uri.c_str());
    if (error == ESP_OK) error = nvs_set_str(handle, kUsername, config.username.c_str());
    if (error == ESP_OK) error = nvs_set_str(handle, kPassword, config.password.c_str());
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

esp_err_t CloudNvsStore::clear() const
{
    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    error = nvs_erase_all(handle);
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

}  // namespace homeguard::idf
