#include "hg_wifi_credentials.hpp"

#include "nvs.h"

#include <cstddef>
#include <cstring>

namespace homeguard::idf {
namespace {
constexpr const char* kNamespace = "hg_wifi";
constexpr const char* kKey = "credentials_v1";
}

esp_err_t WifiCredentialStore::load(WifiCredentials& credentials) const
{
    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (error != ESP_OK) return error;

    std::size_t size = sizeof(credentials);
    error = nvs_get_blob(handle, kKey, &credentials, &size);
    nvs_close(handle);
    if (error != ESP_OK) return error;
    if (size != sizeof(credentials) || !credentials.valid()) return ESP_ERR_INVALID_SIZE;
    credentials.ssid.back() = '\0';
    credentials.password.back() = '\0';
    return ESP_OK;
}

esp_err_t WifiCredentialStore::save(const WifiCredentials& credentials) const
{
    if (!credentials.valid()) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    error = nvs_set_blob(handle, kKey, &credentials, sizeof(credentials));
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

esp_err_t WifiCredentialStore::erase() const
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
