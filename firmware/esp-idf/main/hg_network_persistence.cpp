#include "hg_network_http.hpp"

#include "nvs.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace homeguard::idf {
namespace {

constexpr std::uint32_t kCredentialsMagic = 0x48475746U; // HGWF
constexpr char kNvsNamespace[] = "hg_wifi";
constexpr char kNvsKey[] = "credentials";

struct CredentialsRecord {
    std::uint32_t magic{};
    std::uint8_t ssid_length{};
    std::uint8_t password_length{};
    std::array<char, 32> ssid{};
    std::array<char, 64> password{};
};

}  // namespace

esp_err_t NetworkHttp::snapshot_persisted_credentials(
    std::string& ssid,
    std::string& password,
    bool& present) const
{
    ssid.clear();
    password.clear();
    present = false;

    nvs_handle_t handle{};
    auto error = nvs_open(kNvsNamespace, NVS_READONLY, &handle);
    if (error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (error != ESP_OK) return error;

    CredentialsRecord record{};
    std::size_t size = sizeof(record);
    error = nvs_get_blob(handle, kNvsKey, &record, &size);
    nvs_close(handle);

    if (error == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (error != ESP_OK) return error;
    if (size != sizeof(record) || record.magic != kCredentialsMagic ||
        record.ssid_length == 0 || record.ssid_length > record.ssid.size() ||
        record.password_length > record.password.size()) {
        return ESP_ERR_INVALID_STATE;
    }

    ssid.assign(record.ssid.data(), record.ssid_length);
    password.assign(record.password.data(), record.password_length);
    present = true;
    return ESP_OK;
}

}  // namespace homeguard::idf
