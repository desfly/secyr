#include "hg_access_nvs.hpp"
#include "hg_cloud_nvs.hpp"
#include "hg_commissioning_nvs.hpp"
#include "hg_config_transaction.hpp"
#include "hg_network_http.hpp"

#include "nvs.h"

#include <array>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "Config transaction host test FAIL: " << message << '\n';
        std::exit(1);
    }
}

homeguard::AccessControl make_access(const char* id, const char* name, const char* pin, std::uint8_t salt_byte) {
    homeguard::AccessControl access;
    std::array<std::uint8_t, 16> salt{};
    salt.fill(salt_byte);
    expect(access.set_user(id, name, homeguard::AccessRole::Admin, pin, salt, true),
           "cannot construct access database");
    return access;
}

void expect_access_user(
    const homeguard::idf::AccessNvsStore& store,
    const char* present_id,
    const char* absent_id) {
    homeguard::AccessControl loaded;
    expect(store.load(loaded) == ESP_OK, "cannot reload access database");
    expect(loaded.find_user(present_id) != nullptr, "expected access user missing");
    expect(loaded.find_user(absent_id) == nullptr, "unexpected access user persisted");
}

void expect_wifi(
    const homeguard::idf::NetworkHttp& network,
    const std::string& expected_ssid,
    const std::string& expected_password) {
    std::string ssid;
    std::string password;
    bool present = false;
    expect(network.snapshot_persisted_credentials(ssid, password, present) == ESP_OK,
           "cannot snapshot Wi-Fi credentials");
    expect(present, "Wi-Fi credentials unexpectedly absent");
    expect(ssid == expected_ssid, "Wi-Fi SSID mismatch");
    expect(password == expected_password, "Wi-Fi password mismatch");
}

void expect_cloud(
    const homeguard::idf::CloudNvsStore& store,
    const std::string& broker,
    const std::string& username,
    const std::string& password) {
    homeguard::idf::CloudConfig loaded{};
    expect(store.load(loaded) == ESP_OK, "cannot reload cloud config");
    expect(loaded.enabled, "cloud unexpectedly disabled");
    expect(loaded.broker_uri == broker, "cloud broker mismatch");
    expect(loaded.username == username, "cloud username mismatch");
    expect(loaded.password == password, "cloud password mismatch");
}

}  // namespace

int main() {
    mock_nvs::reset();

    const homeguard::idf::AccessNvsStore access_store;
    const homeguard::idf::NetworkHttp network;
    const homeguard::idf::CloudNvsStore cloud_store;
    const homeguard::idf::CommissioningNvsStore commissioning_store;

    const auto old_access = make_access("oldadmin", "Old Admin", "1234", 0x11U);
    expect(access_store.save(old_access) == ESP_OK, "cannot seed old access state");
    expect(network.save_persisted_credentials("OldWifi", "oldpass88"), "cannot seed old Wi-Fi state");
    const homeguard::idf::CloudConfig old_cloud{
        true,
        "mqtts://old-broker.local:8883",
        "old-user",
        "old-password",
    };
    expect(cloud_store.save(old_cloud) == ESP_OK, "cannot seed old cloud state");

    homeguard::idf::ConfigBackupV1 incoming{};
    incoming.secrets_included = true;
    incoming.access = make_access("newadmin", "New Admin", "5678", 0x22U);
    incoming.wifi_present = true;
    incoming.wifi_ssid = "NewWifi";
    incoming.wifi_password = "newpass88";
    incoming.cloud_present = true;
    incoming.cloud = {
        true,
        "mqtts://new-broker.local:8883",
        "new-user",
        "new-password",
    };
    incoming.commissioning_present = false;

    const homeguard::idf::ConfigTransaction transaction{
        access_store,
        network,
        cloud_store,
        commissioning_store,
    };

    // Access and Wi-Fi are written before Cloud. Failing this first Cloud write
    // therefore exercises rollback of already-mutated persistent state.
    mock_nvs::fail_next_write("hg_cloud", "enabled", ESP_FAIL);
    std::string reason;
    expect(!transaction.apply(incoming, reason), "faulted transaction unexpectedly succeeded");
    expect(reason == "write_cloud_failed", "unexpected rollback failure reason");

    expect_access_user(access_store, "oldadmin", "newadmin");
    expect_wifi(network, "OldWifi", "oldpass88");
    expect_cloud(cloud_store, "mqtts://old-broker.local:8883", "old-user", "old-password");
    hg::CommissioningPersistentState unused{};
    expect(commissioning_store.load_commissioning(unused) == ESP_ERR_NVS_NOT_FOUND,
           "rollback created commissioning state that did not exist before");

    // The fault is one-shot. A normal transaction must now commit the complete
    // new state, proving that rollback safety did not break the success path.
    reason.clear();
    expect(transaction.apply(incoming, reason), "normal transaction failed after rollback test");
    expect(reason.empty(), "successful transaction returned a failure reason");
    expect_access_user(access_store, "newadmin", "oldadmin");
    expect_wifi(network, "NewWifi", "newpass88");
    expect_cloud(cloud_store, "mqtts://new-broker.local:8883", "new-user", "new-password");
    expect(commissioning_store.load_commissioning(unused) == ESP_ERR_NVS_NOT_FOUND,
           "successful import created absent commissioning state");

    std::cout << "Config transaction host test PASS\n";
    return 0;
}
