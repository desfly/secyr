#include "hg_factory_reset.hpp"

#include "esp_wifi.h"
#include "nvs.h"

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "Factory Reset host test FAIL: " << message << '\n';
        std::exit(1);
    }
}

void seed_blob(const char* ns, const char* key, const std::string& value) {
    mock_nvs::put_blob(ns, key, value.data(), value.size());
}

}  // namespace

int main() {
    mock_nvs::reset();

    // Mutable user-owned state covered by acceptance item 20.
    seed_blob("hg_access", "users_v1", "admin-db");
    seed_blob("hg_wifi", "credentials", "ssid-and-password");
    seed_blob("hg_cloud", "config", "broker-and-token");
    seed_blob("hg-config", "config_v1", "controller-config");
    seed_blob("hg-provision", "wifi_pass", "secret-password");
    seed_blob("hg-provision", "owner", "owner-label");
    seed_blob("hg_commission", "state_v1", "commissioning-progress");

    // Immutable identity/hardware state must survive the reset.
    seed_blob("hg_commission", "hardware_v1", "verified-hardware");
    mock_nvs::put_string("hg-factory", "cert_pem", "factory-certificate");
    mock_nvs::put_string("hg-factory", "key_pem", "factory-private-key");
    mock_nvs::put_string("unrelated", "sentinel", "keep-me");

    // Simulate a credential left in legacy ESP-IDF Wi-Fi driver storage. The
    // mock esp_wifi_restore() clears this structure, proving that reset covers
    // both HomeGuard hg_wifi persistence and the driver's old flash state.
    wifi_config_t driver_config{};
    const char legacy_ssid[] = "LegacySSID";
    std::memcpy(driver_config.sta.ssid, legacy_ssid, sizeof(legacy_ssid));
    expect(esp_wifi_set_config(WIFI_IF_STA, &driver_config) == ESP_OK,
           "cannot seed mock Wi-Fi driver config");
    expect(mock_wifi_sta_config().sta.ssid[0] != 0U,
           "mock Wi-Fi driver seed was empty");

    const auto report = homeguard::idf::FactoryResetManager{}.erase_mutable_state();
    expect(report.ok(), "FactoryResetReport is not OK");

    expect(!mock_nvs::has_key("hg_access", "users_v1"), "access users survived");
    expect(!mock_nvs::has_key("hg_wifi", "credentials"), "HomeGuard Wi-Fi credentials survived");
    expect(!mock_nvs::has_key("hg_cloud", "config"), "cloud credentials survived");
    expect(!mock_nvs::has_key("hg-config", "config_v1"), "controller config survived");
    expect(!mock_nvs::has_key("hg-provision", "wifi_pass"), "provisioning Wi-Fi password survived");
    expect(!mock_nvs::has_key("hg-provision", "owner"), "owner label survived");
    expect(!mock_nvs::has_key("hg_commission", "state_v1"), "commissioning progress survived");

    expect(mock_nvs::has_key("hg_commission", "hardware_v1"), "hardware verification was erased");
    expect(mock_nvs::has_key("hg-factory", "cert_pem"), "factory certificate was erased");
    expect(mock_nvs::has_key("hg-factory", "key_pem"), "factory private key was erased");
    expect(mock_nvs::has_key("unrelated", "sentinel"), "unrelated namespace was erased");
    expect(mock_wifi_sta_config().sta.ssid[0] == 0U, "legacy Wi-Fi driver config survived");

    std::cout << "Factory Reset host test PASS\n";
    return 0;
}
