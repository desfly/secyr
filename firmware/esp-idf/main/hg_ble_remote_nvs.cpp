#include "hg_ble_remote_nvs.hpp"

#include "nvs.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace homeguard::idf {
namespace {
constexpr const char* kNamespace = "hg_ble_remote";
constexpr const char* kKey = "bindings_v1";
constexpr std::uint32_t kMagic = 0x48474b46U; // HGKF
constexpr std::uint16_t kVersion = 1;

struct PersistedBinding {
    std::uint8_t address_type{};
    std::array<std::uint8_t, 6> address{};
    std::uint8_t profile{};
    std::array<char, 24> name{};
    std::uint32_t permissions{};
    std::uint8_t enabled{};
    std::uint8_t replay_counter_required{};
    std::uint8_t counter_initialized{};
    std::uint8_t reserved{};
    std::uint32_t last_counter{};
};

struct PersistedImage {
    std::uint32_t magic{kMagic};
    std::uint16_t version{kVersion};
    std::uint16_t count{};
    std::array<PersistedBinding, hg::BleRemoteRegistry::capacity> bindings{};
};

std::string_view binding_name(const hg::BleRemoteBinding& binding)
{
    const auto end = std::find(binding.name.begin(), binding.name.end(), '\0');
    return {binding.name.data(), static_cast<std::size_t>(end - binding.name.begin())};
}
}

esp_err_t BleRemoteNvsStore::load(hg::BleRemoteRegistry& registry) const
{
    nvs_handle_t handle{};
    const auto open_error = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (open_error != ESP_OK) return open_error;

    PersistedImage image{};
    std::size_t size = sizeof(image);
    const auto read_error = nvs_get_blob(handle, kKey, &image, &size);
    nvs_close(handle);
    if (read_error != ESP_OK) return read_error;
    if (size != sizeof(image) || image.magic != kMagic || image.version != kVersion ||
        image.count > hg::BleRemoteRegistry::capacity) {
        return ESP_ERR_INVALID_SIZE;
    }

    hg::BleRemoteRegistry restored{};
    for (std::size_t i = 0; i < image.count; ++i) {
        const auto& source = image.bindings[i];
        hg::BleRemoteIdentity identity{};
        identity.address_type = source.address_type;
        identity.address = source.address;
        const auto profile = static_cast<hg::BleRemoteProfile>(source.profile);
        const auto name_end = std::find(source.name.begin(), source.name.end(), '\0');
        const std::string_view name{source.name.data(), static_cast<std::size_t>(name_end - source.name.begin())};
        if (name.empty() || source.permissions == 0U ||
            !restored.bind(identity, profile, name, source.permissions, source.replay_counter_required != 0U)) {
            return ESP_ERR_INVALID_STATE;
        }
        auto* target = restored.find(identity);
        if (target == nullptr) return ESP_ERR_INVALID_STATE;
        target->enabled = source.enabled != 0U;
        target->counter_initialized = source.counter_initialized != 0U;
        target->last_counter = source.last_counter;
    }

    registry = restored;
    return ESP_OK;
}

esp_err_t BleRemoteNvsStore::save(const hg::BleRemoteRegistry& registry) const
{
    PersistedImage image{};
    image.count = static_cast<std::uint16_t>(registry.size());
    for (std::size_t i = 0; i < registry.size(); ++i) {
        const auto* source = registry.binding_at(i);
        if (source == nullptr) return ESP_ERR_INVALID_STATE;
        auto& target = image.bindings[i];
        target.address_type = source->identity.address_type;
        target.address = source->identity.address;
        target.profile = static_cast<std::uint8_t>(source->profile);
        target.name = source->name;
        target.permissions = source->permissions;
        target.enabled = source->enabled ? 1U : 0U;
        target.replay_counter_required = source->replay_counter_required ? 1U : 0U;
        target.counter_initialized = source->counter_initialized ? 1U : 0U;
        target.last_counter = source->last_counter;
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
