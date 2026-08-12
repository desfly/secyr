#include "hg_cloud_nvs.hpp"

#include "nvs.h"

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace homeguard::idf {
namespace {
constexpr char kNamespace[] = "hg_cloud";
constexpr char kKey[] = "mqtt_cfg";
constexpr std::uint32_t kMagic = 0x4847434cU;  // HGCL

struct PersistedCloudConfig {
    std::uint32_t magic{};
    CloudConfigRecord config{};
};

bool terminated(const auto& value)
{
    return std::find(value.begin(), value.end(), '\0') != value.end();
}

bool valid(const CloudConfigRecord& config)
{
    if (!terminated(config.broker_uri) || !terminated(config.username) || !terminated(config.password)) return false;
    if (config.enabled && config.broker_uri[0] == '\0') return false;
    return true;
}
}

esp_err_t CloudNvsStore::load(CloudConfigRecord& config) const
{
    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (error != ESP_OK) return error;

    PersistedCloudConfig stored{};
    std::size_t size = sizeof(stored);
    error = nvs_get_blob(handle, kKey, &stored, &size);
    nvs_close(handle);
    if (error != ESP_OK) return error;
    if (size != sizeof(stored) || stored.magic != kMagic || !valid(stored.config)) return ESP_ERR_INVALID_CRC;

    config = stored.config;
    return ESP_OK;
}

esp_err_t CloudNvsStore::save(const CloudConfigRecord& config) const
{
    if (!valid(config)) return ESP_ERR_INVALID_ARG;

    PersistedCloudConfig stored{};
    stored.magic = kMagic;
    stored.config = config;

    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    error = nvs_set_blob(handle, kKey, &stored, sizeof(stored));
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

esp_err_t CloudNvsStore::clear() const
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
