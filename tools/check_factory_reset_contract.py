#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "esp-idf" / "main"

manager = (MAIN / "hg_factory_reset.cpp").read_text(encoding="utf-8")
manager_h = (MAIN / "hg_factory_reset.hpp").read_text(encoding="utf-8")
commissioning = (MAIN / "hg_commissioning_nvs.cpp").read_text(encoding="utf-8")
system = (MAIN / "hg_system_http.cpp").read_text(encoding="utf-8")
cmake = (MAIN / "CMakeLists.txt").read_text(encoding="utf-8")

checks = {
    "Factory Reset manager compiled": '"hg_factory_reset.cpp"' in cmake,
    "Factory Reset report is explicit": "struct FactoryResetReport" in manager_h and "bool ok() const" in manager_h,
    "Access state erased": 'erase_namespace("hg_access")' in manager,
    "Wi-Fi state erased": 'erase_namespace("hg_wifi")' in manager,
    "Cloud state erased": 'erase_namespace("hg_cloud")' in manager,
    "Commissioning user state erased selectively": "erase_commissioning_state()" in manager,
    "Hardware verification preserved": 'kHardwareKey' in commissioning and 'erase_one(kCommissioningKey)' in commissioning and 'erase_one(kHardwareKey)' not in commissioning,
    "Factory Reset API route exists": '"/api/v1/system/factory-reset"' in system,
    "Factory Reset requires credentials": 'parse_json_string(body, "actor", actor)' in system and 'parse_json_string(body, "credential", credential)' in system,
    "Factory Reset requires explicit destructive confirmation": 'confirm != "ERASE_ALL"' in system and 'explicit_confirmation_required' in system,
    "Factory Reset is Admin-authorized": 'authorize(actor, credential, "system.factory_reset")' in system,
    "Factory Reset clears live access state": "access_control_->clear_users();" in system,
    "Factory Reset reboots only after successful erase": "if (!report.ok())" in system and "delayed_factory_reboot" in system and "esp_restart();" in system,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(("OK   " if ok else "FAIL ") + name)

if failed:
    raise SystemExit("Factory Reset contract failed: " + ", ".join(failed))

print("Factory Reset contract PASS")
