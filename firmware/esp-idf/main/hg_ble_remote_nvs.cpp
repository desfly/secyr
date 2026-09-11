#include "hg_ble_remote_nvs.hpp"

#include "nvs.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace homeguard::idf {
namespace {
constexpr const char* kNamespace = "hg_remotes";
constexpr const char* kKey = "bindings_v1";
constexpr std::uint32_t kMagic = 0x48524742U; // HRGB
constexpr std::uint16_t kVersion = 1U;

struct RemoteImage {
    std::uint32_t magic{kMagic};
    std::uint16_t version{kVersion};
    std::uint16_t count{};
    std::array<hg::BleRemoteBinding, hg::BleRemoteRegistry::kMaxBindings> bindings{};
};
}

esp_err_t BleRemoteNvsStore::load(hg::BleRemoteRegistry& registry) const
{
    nvs_handle_t handle{};
    const auto open_error = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (open_error != ESP_OK) return open_error;

    RemoteImage image{};
    std::size_t size = sizeof(image);
    const auto error = nvs_get_blob(handle, kKey, &image, &size);
    nvs_close(handle);
    if (error != ESP_OK) return error;
    if (size != sizeof(image) || image.magic != kMagic || image.version != kVersion ||
        image.count > hg::BleRemoteRegistry::kMaxBindings) return ESP_ERR_INVALID_SIZE;

    hg::BleRemoteRegistry restored;
    std::size_t imported = 0;
    for (const auto& binding : image.bindings) {
        if (!binding.occupied) continue;
        if (!restored.import_binding(binding)) return ESP_ERR_INVALID_STATE;
        ++imported;
    }
    if (imported != image.count) return ESP_ERR_INVALID_STATE;
    registry = restored;
    return ESP_OK;
}

esp_err_t BleRemoteNvsStore::save(const hg::BleRemoteRegistry& registry) const
{
    RemoteImage image{};
    for (std::size_t index = 0; index < hg::BleRemoteRegistry::kMaxBindings; ++index) {
        if (const auto* binding = registry.binding_at(index); binding != nullptr) {
            image.bindings[index] = *binding;
            ++image.count;
        }
    }

    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    error = nvs_set_blob(handle, kKey, &image, sizeof(image));
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

esp_err_t BleRemoteNvsStore::erase() const
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
