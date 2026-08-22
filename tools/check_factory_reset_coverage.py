from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "esp-idf" / "main"
COMPONENTS = ROOT / "firmware" / "esp-idf" / "components"

errors = []

factory_reset = (MAIN / "hg_factory_reset.cpp").read_text(encoding="utf-8")
factory_header = (MAIN / "hg_factory_reset.hpp").read_text(encoding="utf-8")
commissioning = (MAIN / "hg_commissioning_nvs.cpp").read_text(encoding="utf-8")
config_store = (COMPONENTS / "nvs_config_store" / "nvs_config_store.cpp").read_text(encoding="utf-8")

# Full user factory reset must erase every mutable user-owned namespace.
for namespace in ("hg_access", "hg_wifi", "hg_cloud", "hg-config", "hg-provision"):
    if f'erase_namespace("{namespace}")' not in factory_reset:
        errors.append(f"factory reset must erase mutable namespace {namespace}")

# Settings reset is intentionally narrower: network/cloud/controller/provisioning
# and commissioning progress are cleared, but access users must survive.
if "erase_settings_state()" not in factory_header or "FactoryResetManager::erase_settings_state()" not in factory_reset:
    errors.append("settings reset API is missing")
settings_start = factory_reset.find("FactoryResetReport FactoryResetManager::erase_settings_state()")
factory_start = factory_reset.find("FactoryResetReport FactoryResetManager::erase_mutable_state()")
settings_body = factory_reset[settings_start:factory_start] if settings_start >= 0 and factory_start > settings_start else ""
if 'erase_namespace("hg_access")' in settings_body:
    errors.append("settings reset must preserve hg_access users")
if "erase_settings_namespaces()" not in settings_body:
    errors.append("settings reset must clear the shared mutable settings boundary")

full_body = factory_reset[factory_start:] if factory_start >= 0 else ""
if 'erase_namespace("hg_access")' not in full_body:
    errors.append("full factory reset must additionally erase hg_access users")

# Commissioning progress is mutable, while hardware verification is immutable factory evidence.
if "CommissioningNvsStore{}.erase_commissioning_state()" not in factory_reset:
    errors.append("settings/factory reset must erase commissioning progress")
if 'kHardwareKey = "hardware_v1"' not in commissioning or 'kCommissioningKey = "state_v1"' not in commissioning:
    errors.append("commissioning storage keys changed; review reset preservation boundary")
if "erase_commissioning_state()" not in commissioning:
    errors.append("commissioning store must support state-only erase")

# Factory identity and hardware verification survive both user reset levels.
if 'factory_namespace[] = "hg-factory"' not in config_store:
    errors.append("factory identity namespace changed; review reset preservation boundary")
if 'erase_namespace("hg-factory")' in factory_reset:
    errors.append("user reset must preserve hg-factory identity")
if "erase_all()" in factory_reset and "CommissioningNvsStore" in factory_reset:
    errors.append("user reset must not erase immutable commissioning hardware verification")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("Settings/factory reset namespace coverage audit PASS")
print(" - settings reset preserves hg_access and immutable device identity")
print(" - full factory reset additionally erases hg_access")

# Physical RST/EN + RGB behavior is release-critical and gates BIN generation.
rst_rgb = subprocess.run(
    [sys.executable, str(ROOT / "tools" / "check_reset_rgb_contract.py")],
    cwd=ROOT,
    check=False,
)
if rst_rgb.returncode != 0:
    sys.exit(rst_rgb.returncode)
