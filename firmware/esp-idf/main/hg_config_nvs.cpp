#include "hg_config_nvs.hpp"

#include "nvs.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace homeguard::idf {
namespace {
constexpr char kNamespace[] = "hg_cfgx";
constexpr char kKey[] = "config_json";
constexpr std::size_t kMaxConfigBytes = 32768;
}

esp_err_t ConfigNvsStore::load(HomeGuardConfigDocument& document) const
{
    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (error != ESP_OK) return error;

    std::size_t size = 0;
    error = nvs_get_blob(handle, kKey, nullptr, &size);
    if (error != ESP_OK) { nvs_close(handle); return error; }
    if (size == 0 || size > kMaxConfigBytes) { nvs_close(handle); return ESP_ERR_INVALID_SIZE; }

    std::vector<char> bytes(size);
    error = nvs_get_blob(handle, kKey, bytes.data(), &size);
    nvs_close(handle);
    if (error != ESP_OK) return error;

    const std::string_view json{bytes.data(), size};
    HomeGuardConfigDocument candidate{};
    const auto imported = import_config_json(json, candidate);
    if (!imported.ok()) return ESP_ERR_INVALID_CRC;
    document = candidate;
    return ESP_OK;
}

esp_err_t ConfigNvsStore::save(const HomeGuardConfigDocument& document) const
{
    const auto validation = validate_config_document(document);
    if (!validation.ok()) return ESP_ERR_INVALID_ARG;
    const std::string json = export_config_json(document);
    if (json.empty() || json.size() > kMaxConfigBytes) return ESP_ERR_INVALID_SIZE;

    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    error = nvs_set_blob(handle, kKey, json.data(), json.size());
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

}  // namespace homeguard::idf
