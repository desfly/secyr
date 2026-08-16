#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "esp-idf" / "main"
CONFIG_STORE = ROOT / "firmware" / "esp-idf" / "components" / "nvs_config_store"

manager = (MAIN / "hg_factory_reset.cpp").read_text(encoding="utf-8")
manager_h = (MAIN / "hg_factory_reset.hpp").read_text(encoding="utf-8")
commissioning = (MAIN / "hg_commissioning_nvs.cpp").read_text(encoding="utf-8")
system = (MAIN / "hg_system_http.cpp").read_text(encoding="utf-8")
network = (MAIN / "hg_network_http.cpp").read_text(encoding="utf-8")
access_nvs = (MAIN / "hg_access_nvs.cpp").read_text(encoding="utf-8")
app_main = (MAIN / "app_main.cpp").read_text(encoding="utf-8")
cmake = (MAIN / "CMakeLists.txt").read_text(encoding="utf-8")
config_store = (CONFIG_STORE / "nvs_config_store.cpp").read_text(encoding="utf-8")

checks = {
    "Factory Reset manager compiled": '"hg_factory_reset.cpp"' in cmake,
    "Factory Reset report is explicit": "struct FactoryResetReport" in manager_h and "bool ok() const" in manager_h,
    "Access state erased": 'erase_namespace("hg_access")' in manager,
    "Wi-Fi HomeGuard state erased": 'erase_namespace("hg_wifi")' in manager,
    "Wi-Fi driver persistence erased": "esp_wifi_restore();" in manager,
    "Wi-Fi runtime config is RAM-only": "esp_wifi_set_storage(WIFI_STORAGE_RAM)" in network,
    "Cloud state erased": 'erase_namespace("hg_cloud")' in manager,
    "Controller config erased": 'erase_namespace("hg-config")' in manager,
    "Provisioning payload erased": 'erase_namespace("hg-provision")' in manager,
    "Immutable factory identity is preserved": 'erase_namespace("hg-factory")' not in manager and 'factory_namespace[] = "hg-factory"' in config_store,
    "Reset report tracks controller config": "controller_config" in manager_h,
    "Reset report tracks provisioning": "provisioning" in manager_h,
    "Commissioning user state erased selectively": "erase_commissioning_state()" in manager,
    "Factory Reset never calls commissioning erase_all": ".erase_all()" not in manager,
    "Hardware verification key remains separate": 'kHardwareKey = "hardware_v1"' in commissioning,
    "Selective commissioning erase targets only state_v1": "return erase_key(kCommissioningKey);" in commissioning,

    # Acceptance item 22: clearing hg_access must become factory-first-Admin
    # after reboot. Guard the exact storage->boot decision chain.
    "Access store reads factory namespace": 'constexpr const char* nvs_namespace = "hg_access"' in access_nvs,
    "Missing access namespace stays NOT_FOUND": "if (open_error == ESP_ERR_NVS_NOT_FOUND) return open_error;" in access_nvs,
    "Missing access key stays NOT_FOUND": "if (read_error != ESP_OK) return read_error;" in access_nvs,
    "Boot starts bootstrap fail-closed": "g_access_bootstrap_allowed = false;" in app_main,
    "Boot enables bootstrap only for missing access DB": "if (error == ESP_ERR_NVS_NOT_FOUND)" in app_main and "g_access_bootstrap_allowed = true;" in app_main,
    "Boot logs factory first-Admin state": "factory first-Admin bootstrap enabled" in app_main,
    "Corrupt access DB does not reopen bootstrap": "Access database rejected" in app_main and "bootstrap stays disabled" in app_main,
    "HTTP access layer receives boot bootstrap state": "g_access_http.register_handlers(g_http_server, &g_access_control, &g_access_store, g_access_bootstrap_allowed)" in app_main,

    "Factory Reset API route exists": '"/api/v1/system/factory-reset"' in system,
    "Control requests are fully read": "read_request_body" in system and "while (offset < body.size())" in system and "offset += static_cast<std::size_t>(received);" in system,
    "Factory Reset uses complete bounded body": "read_request_body(request, 512U, body)" in system,
    "Security command uses complete bounded body": "read_request_body(request, 384U, body)" in system,
    "Factory Reset requires credentials": 'parse_json_string(body, "actor", actor)' in system and 'parse_json_string(body, "credential", credential)' in system,
    "Factory Reset requires explicit destructive confirmation": 'confirm != "ERASE_ALL"' in system and 'explicit_confirmation_required' in system,
    "Factory Reset is Admin-authorized": 'authorize(actor, credential, "system.factory_reset")' in system,
    "Factory Reset clears live access state": "access_control_->clear_users();" in system,
    "Reset failure reports every mutable namespace": all(key in system for key in ('\\"access\\"', '\\"wifi\\"', '\\"cloud\\"', '\\"controllerConfig\\"', '\\"provisioning\\"', '\\"commissioning\\"')),
    "Factory Reset reboots only after successful erase": "if (!report.ok())" in system and "delayed_factory_reboot" in system and "esp_restart();" in system,
    "Reboot is scheduled before success response": system.find("xTaskCreate(") != -1 and system.find("xTaskCreate(") < system.find("factory_reset_complete"),
    "Socket write cannot cancel reboot": "if (send_error == ESP_OK)" not in system and "return send_json(request, response" in system,
    "Reboot scheduler failure is fail-safe": "if (reboot_task != pdPASS)" in system and system.count("esp_restart();") >= 2,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(("OK   " if ok else "FAIL ") + name)

if failed:
    raise SystemExit("Factory Reset contract failed: " + ", ".join(failed))

print("Factory Reset contract PASS")
print(" - Wi-Fi persistence: hg_wifi + legacy ESP-IDF settings erased; runtime driver config RAM-only")
print(" - reboot factory-fresh chain: missing hg_access -> bootstrap allowed; corrupt DB remains fail-closed")
