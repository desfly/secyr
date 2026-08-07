#include "hg_access_nvs.hpp"
#include "homeguard/access_store.hpp"

#include "nvs.h"

#include <cstddef>

namespace homeguard::idf {
namespace {
constexpr const char* nvs_namespace = "hg_access";
constexpr const char* nvs_key = "users_v1";
}

esp_err_t AccessNvsStore::load(AccessControl& access) const {
    nvs_handle_t handle{};
    const auto open_error = nvs_open(nvs_namespace, NVS_READONLY, &handle);
    if (open_error == ESP_ERR_NVS_NOT_FOUND) return open_error;
    if (open_error != ESP_OK) return open_error;

    AccessStoreCodec::Image image{};
    std::size_t size = image.size();
    const auto read_error = nvs_get_blob(handle, nvs_key, image.data(), &size);
    nvs_close(handle);
    if (read_error != ESP_OK) return read_error;
    if (size != image.size()) return ESP_ERR_INVALID_SIZE;
    return AccessStoreCodec::decode(image, access) ? ESP_OK : ESP_ERR_INVALID_CRC;
}

esp_err_t AccessNvsStore::save(const AccessControl& access) const {
    nvs_handle_t handle{};
    auto error = nvs_open(nvs_namespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;

    const auto image = AccessStoreCodec::encode(access);
    error = nvs_set_blob(handle, nvs_key, image.data(), image.size());
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

esp_err_t AccessNvsStore::erase() const {
    nvs_handle_t handle{};
    auto error = nvs_open(nvs_namespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    error = nvs_erase_key(handle, nvs_key);
    if (error == ESP_ERR_NVS_NOT_FOUND) error = ESP_OK;
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

}  // namespace homeguard::idf
