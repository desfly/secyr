#include "hg_cloud_config.hpp"
#include "nvs.h"

#include <cstddef>

namespace homeguard::idf {
namespace {
constexpr const char* kNamespace = "hg_cloud";
constexpr const char* kKey = "config_v1";
}

esp_err_t CloudConfigStore::load(CloudConfig& config) const
{
    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (error != ESP_OK) return error;
    std::size_t size = sizeof(config);
    error = nvs_get_blob(handle, kKey, &config, &size);
    nvs_close(handle);
    if (error != ESP_OK) return error;
    if (size != sizeof(config) || !config.valid()) return ESP_ERR_INVALID_SIZE;
    config.broker_uri.back() = '\0';
    config.username.back() = '\0';
    config.password.back() = '\0';
    return ESP_OK;
}

esp_err_t CloudConfigStore::save(const CloudConfig& config) const
{
    if (!config.valid()) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    error = nvs_set_blob(handle, kKey, &config, sizeof(config));
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

esp_err_t CloudConfigStore::erase() const
{
    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    error = nvs_erase_key(handle, kKey);
    if (error == ESP_ERR_NVS_NOT_FOUND) error = ESP_OK;
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

}  // namespace homeguard::idf
