#!/usr/bin/env python3
"""Release guard for transactional configuration import safety."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "firmware"
MAIN = FIRMWARE / "esp-idf" / "main"
TESTS = ROOT / "tests"

codec = (MAIN / "hg_config_backup.cpp").read_text(encoding="utf-8")
transaction = (MAIN / "hg_config_transaction.cpp").read_text(encoding="utf-8")
config_http = (MAIN / "hg_config_http.cpp").read_text(encoding="utf-8")
host_cmake = (FIRMWARE / "CMakeLists.txt").read_text(encoding="utf-8")
host_test = (TESTS / "test_config_transaction_host.cpp").read_text(encoding="utf-8")
nvs_mock = (TESTS / "esp-idf-mock/include/nvs.h").read_text(encoding="utf-8")

checks = {
    "Codec rejects access image without enabled Admin":
        'backup.access.enabled_admin_count() == 0U' in codec and 'fail("access_admin_required")' in codec,
    "Transaction independently rejects no-Admin backup":
        'backup.access.enabled_admin_count() == 0U' in transaction and 'reason = "access_admin_required"' in transaction,
    "No-Admin rejection happens before snapshot/write":
        transaction.find('backup.access.enabled_admin_count() == 0U') < transaction.find('Snapshot before{}'),
    "Transaction captures old Access/Wi-Fi/Cloud/commissioning":
        all(token in transaction for token in (
            'access_store_.load(snapshot.access)',
            'snapshot_persisted_credentials(',
            'cloud_store_.load(snapshot.cloud)',
            'commissioning_store_.load_commissioning(snapshot.commissioning)',
        )),
    "Transaction rollback restores every captured store":
        all(token in transaction for token in (
            'access_store_.save(snapshot.access)',
            'network_.save_persisted_credentials(snapshot.wifi_ssid, snapshot.wifi_password)',
            'cloud_store_.save(snapshot.cloud)',
            'commissioning_store_.save_commissioning(snapshot.commissioning)',
        )),
    "Every mutating import step rolls back on failure":
        transaction.count('return rollback(') >= 8,
    "Rollback failure is surfaced distinctly":
        ':rollback_failed' in transaction,
    "NVS mock supports one-shot targeted write fault":
        'inline void fail_next_write(' in nvs_mock and 'consume_write_fault' in nvs_mock and 'write_fault.armed = false' in nvs_mock,
    "Config transaction executable exists":
        'add_executable(config_transaction_host_test' in host_cmake,
    "Config transaction target compiles production transaction":
        'esp-idf/main/hg_config_transaction.cpp' in host_cmake,
    "Config transaction target compiles real persistence stores":
        all(token in host_cmake for token in (
            'esp-idf/main/hg_access_nvs.cpp',
            'esp-idf/main/hg_cloud_nvs.cpp',
            'esp-idf/main/hg_commissioning_nvs.cpp',
            'esp-idf/main/hg_network_http.cpp',
            'esp-idf/main/hg_network_persistence.cpp',
        )),
    "Config transaction test is registered in CTest":
        'add_test(NAME config_transaction_host_test COMMAND config_transaction_host_test)' in host_cmake,
    "Executable test rejects lockout backup":
        'reason == "access_admin_required"' in host_test and 'Admin-less transaction unexpectedly succeeded' in host_test,
    "Executable test injects Cloud failure after Access/Wi-Fi writes":
        'fail_next_write("hg_cloud", "enabled", ESP_FAIL)' in host_test and 'reason == "write_cloud_failed"' in host_test,
    "Executable test proves old Access restored":
        'expect_access_user(access_store, "oldadmin", "newadmin")' in host_test,
    "Executable test proves old Wi-Fi restored":
        'expect_wifi(network, "OldWifi", "oldpass88")' in host_test,
    "Executable test proves old Cloud restored":
        'expect_cloud(cloud_store, "mqtts://old-broker.local:8883", "old-user", "old-password")' in host_test,
    "Executable test also proves success path":
        'transaction.apply(incoming, reason)' in host_test and 'expect_access_user(access_store, "newadmin", "oldadmin")' in host_test,
    "Config import uses transaction rather than direct writes":
        'ConfigTransaction transaction' in config_http and 'transaction.apply(backup, reason)' in config_http,
    "Successful config import announces reboot":
        '"rebooting\\":true' in config_http and 'delayed_config_reboot' in config_http,
    "Config import schedules reboot before response":
        config_http.find('xTaskCreate(') != -1 and config_http.find('xTaskCreate(') < config_http.find('config_imported'),
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(("OK   " if ok else "FAIL ") + name)

if failed:
    raise SystemExit("Config transaction contract failed: " + ", ".join(failed))

print("Config transaction contract PASS")
print(" - codec + transaction reject Admin-less imports before persistence")
print(" - executable CTest fault-injects mid-transaction and proves rollback")
print(" - executable CTest also proves normal commit path")
print(" - successful HTTP import schedules reboot before response")
