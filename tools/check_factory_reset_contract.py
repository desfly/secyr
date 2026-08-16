#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "esp-idf" / "main"
CONFIG_STORE = ROOT / "firmware" / "esp-idf" / "components" / "nvs_config_store"

manager = (MAIN / "hg_factory_reset.cpp").read_text(encoding="utf-8")
manager_h = (MAIN / "hg_factory_reset.hpp").read_text(encoding="utf-8")
commissioning = (MAIN / "hg_commissioning_nvs.cpp").read_text(encoding="utf-8")
system = (MAIN / "hg_system_http.cpp").read_text(encoding="utf-8")
cmake = (MAIN / "CMakeLists.txt").read_text(encoding="utf-8")
config_store = (CONFIG_STORE / "nvs_config_store.cpp").read_text(encoding="utf-8")

checks = {
    "Factory Reset manager compiled": '"hg_factory_reset.cpp"' in cmake,
    "Factory Reset report is explicit": "struct FactoryResetReport" in manager_h and "bool ok() const" in manager_h,
    "Access state erased": 'erase_namespace("hg_access")' in manager,
    "Wi-Fi state erased": 'erase_namespace("hg_wifi")' in manager,
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
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(("OK   " if ok else "FAIL ") + name)

if failed:
    raise SystemExit("Factory Reset contract failed: " + ", ".join(failed))

print("Factory Reset contract PASS")
