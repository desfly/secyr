#include "nvs_config_store.hpp"
#include "nvs.h"
#include <algorithm>
#include <array>
#include <string>
#include <utility>

namespace {
constexpr char config_namespace[] = "hg-config";
constexpr char provision_namespace[] = "hg-provision";
constexpr char factory_namespace[] = "hg-factory";

bool get_string(nvs_handle_t handle, const char* key, std::string& value) {
    size_t length = 0;
    if (nvs_get_str(handle, key, nullptr, &length) != ESP_OK || length == 0U) return false;
    std::string buffer(length, '\0');
    if (nvs_get_str(handle, key, buffer.data(), &length) != ESP_OK) return false;
    if (!buffer.empty() && buffer.back() == '\0') buffer.pop_back();
    value = std::move(buffer);
    return true;
}

bool set_string(nvs_handle_t handle, const char* key, const std::string& value) {
    return nvs_set_str(handle, key, value.c_str()) == ESP_OK;
}
}

bool NvsConfigStore::load(hg::ControllerConfig& config) {
    nvs_handle_t handle{};
    if (nvs_open(config_namespace, NVS_READONLY, &handle) != ESP_OK) return false;
    size_t length = sizeof(config);
    const bool ok = nvs_get_blob(handle, "controller", &config, &length) == ESP_OK && length == sizeof(config);
    nvs_close(handle);
    return ok;
}

bool NvsConfigStore::save(const hg::ControllerConfig& config) {
    nvs_handle_t handle{};
    if (nvs_open(config_namespace, NVS_READWRITE, &handle) != ESP_OK) return false;
    const bool ok = nvs_set_blob(handle, "controller", &config, sizeof(config)) == ESP_OK && nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
    return ok;
}

bool NvsConfigStore::is_provisioned() const {
    nvs_handle_t handle{};
    if (nvs_open(provision_namespace, NVS_READONLY, &handle) != ESP_OK) return false;
    uint8_t value = 0;
    const bool ok = nvs_get_u8(handle, "complete", &value) == ESP_OK && value == 1U;
    nvs_close(handle);
    return ok;
}

bool NvsConfigStore::load_provisioning(hg::ProvisioningPayload& payload) const {
    nvs_handle_t handle{};
    if (nvs_open(provision_namespace, NVS_READONLY, &handle) != ESP_OK) return false;
    uint8_t complete = 0;
    const bool ok = nvs_get_u8(handle, "complete", &complete) == ESP_OK && complete == 1U &&
        get_string(handle, "wifi_ssid", payload.wifi_ssid) &&
        get_string(handle, "wifi_pass", payload.wifi_password) &&
        get_string(handle, "api_token", payload.local_api_token);
    if (ok) {
        get_string(handle, "cloud_uri", payload.cloud_endpoint);
        get_string(handle, "cloud_token", payload.cloud_token);
        get_string(handle, "owner", payload.owner_label);
    }
    nvs_close(handle);
    return ok;
}

bool NvsConfigStore::save_provisioning(const hg::ProvisioningPayload& payload) {
#if !defined(CONFIG_NVS_ENCRYPTION) || !CONFIG_NVS_ENCRYPTION
    // Bench-test builds must never persist Wi-Fi passwords, API tokens, or
    // cloud credentials as plaintext. Secure provisioning remains disabled
    // until the production HMAC/eFuse allocation is explicitly approved.
    (void)payload;
    return false;
#else
    nvs_handle_t handle{};
    if (nvs_open(provision_namespace, NVS_READWRITE, &handle) != ESP_OK) return false;
    bool ok = nvs_erase_all(handle) == ESP_OK;
    ok = ok && set_string(handle, "wifi_ssid", payload.wifi_ssid);
    ok = ok && set_string(handle, "wifi_pass", payload.wifi_password);
    ok = ok && set_string(handle, "cloud_uri", payload.cloud_endpoint);
    ok = ok && set_string(handle, "cloud_token", payload.cloud_token);
    ok = ok && set_string(handle, "api_token", payload.local_api_token);
    ok = ok && set_string(handle, "owner", payload.owner_label);
    ok = ok && nvs_set_u8(handle, "complete", 1U) == ESP_OK;
    ok = ok && nvs_commit(handle) == ESP_OK;
    if (!ok) nvs_erase_all(handle);
    nvs_close(handle);
    return ok;
#endif
}

bool NvsConfigStore::erase_provisioning(bool physical_presence) {
    if (!physical_presence) return false;
    nvs_handle_t handle{};
    if (nvs_open(provision_namespace, NVS_READWRITE, &handle) != ESP_OK) return false;
    const bool ok = nvs_erase_all(handle) == ESP_OK && nvs_commit(handle) == ESP_OK;
    nvs_close(handle);
    return ok;
}


bool FactoryProvisioningIdentity::valid() const {
    return !certificate_pem.empty() && !private_key_pem.empty() &&
        hg::valid_sha256_hex(certificate_sha256) && hg::valid_pairing_code(pairing_code) &&
        setup_password.size() >= 12U && setup_password.size() <= 63U;
}

void FactoryProvisioningIdentity::clear_private_material() {
    std::fill(private_key_pem.begin(), private_key_pem.end(), '\0');
    std::fill(pairing_code.begin(), pairing_code.end(), '\0');
    std::fill(setup_password.begin(), setup_password.end(), '\0');
    private_key_pem.clear(); pairing_code.clear(); setup_password.clear();
}

bool NvsConfigStore::load_factory_identity(FactoryProvisioningIdentity& identity) const {
    nvs_handle_t handle{};
    if (nvs_open(factory_namespace, NVS_READONLY, &handle) != ESP_OK) return false;
    const bool ok = get_string(handle, "cert_pem", identity.certificate_pem) &&
        get_string(handle, "key_pem", identity.private_key_pem) &&
        get_string(handle, "cert_sha", identity.certificate_sha256) &&
        get_string(handle, "pair_code", identity.pairing_code) &&
        get_string(handle, "setup_pass", identity.setup_password);
    nvs_close(handle);
    return ok && identity.valid();
}
