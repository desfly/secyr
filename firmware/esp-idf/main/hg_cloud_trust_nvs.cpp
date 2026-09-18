#include "hg_cloud_trust_nvs.hpp"

#include "nvs.h"

#include <array>
#include <cstddef>

namespace homeguard::idf {
namespace {
constexpr char kNamespace[] = "hg_cloudtrust";
constexpr char kVersion[] = "version";
constexpr char kPublicKey[] = "pubkey";
constexpr std::size_t kMaxPublicKeyPem = 2048;

esp_err_t get_public_key(nvs_handle_t handle, std::string& out)
{
    std::size_t size = 0;
    auto error = nvs_get_str(handle, kPublicKey, nullptr, &size);
    if (error != ESP_OK) return error;
    if (size < 2 || size > kMaxPublicKeyPem + 1) return ESP_ERR_INVALID_SIZE;
    std::array<char, kMaxPublicKeyPem + 1> buffer{};
    error = nvs_get_str(handle, kPublicKey, buffer.data(), &size);
    if (error == ESP_OK) out.assign(buffer.data());
    return error;
}
}

esp_err_t CloudTrustStore::load(CloudCommandTrust& trust) const
{
    trust = {};
    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READONLY, &handle);
    if (error != ESP_OK) return error;
    error = nvs_get_u32(handle, kVersion, &trust.version);
    if (error == ESP_OK) error = get_public_key(handle, trust.public_key_pem);
    nvs_close(handle);
    if (error != ESP_OK) trust = {};
    return error;
}

esp_err_t CloudTrustStore::save(const CloudCommandTrust& trust) const
{
    if (trust.version == 0 || trust.public_key_pem.empty() ||
        trust.public_key_pem.size() > kMaxPublicKeyPem) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle{};
    auto error = nvs_open(kNamespace, NVS_READWRITE, &handle);
    if (error != ESP_OK) return error;
    error = nvs_set_u32(handle, kVersion, trust.version);
    if (error == ESP_OK) error = nvs_set_str(handle, kPublicKey, trust.public_key_pem.c_str());
    if (error == ESP_OK) error = nvs_commit(handle);
    nvs_close(handle);
    return error;
}

esp_err_t CloudTrustStore::clear() const
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
