from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "esp-idf" / "main"
COMPONENTS = ROOT / "firmware" / "esp-idf" / "components"

errors = []

factory_reset = (MAIN / "hg_factory_reset.cpp").read_text(encoding="utf-8")
commissioning = (MAIN / "hg_commissioning_nvs.cpp").read_text(encoding="utf-8")
config_store = (COMPONENTS / "nvs_config_store" / "nvs_config_store.cpp").read_text(encoding="utf-8")

# Every mutable user-owned namespace must be explicitly erased by factory reset.
for namespace in ("hg_access", "hg_wifi", "hg_cloud", "hg-config", "hg-provision"):
    if f'erase_namespace("{namespace}")' not in factory_reset:
        errors.append(f"factory reset must erase mutable namespace {namespace}")

# Commissioning progress is mutable, while the hardware verification record is immutable factory evidence.
if "CommissioningNvsStore{}.erase_commissioning_state()" not in factory_reset:
    errors.append("factory reset must erase commissioning progress")
if 'kHardwareKey = "hardware_v1"' not in commissioning or 'kCommissioningKey = "state_v1"' not in commissioning:
    errors.append("commissioning storage keys changed; review reset preservation boundary")
if "erase_commissioning_state()" not in commissioning:
    errors.append("commissioning store must support state-only erase")

# Factory identity must survive a user factory reset.
if 'factory_namespace[] = "hg-factory"' not in config_store:
    errors.append("factory identity namespace changed; review reset preservation boundary")
if 'erase_namespace("hg-factory")' in factory_reset:
    errors.append("factory reset must preserve hg-factory identity")
if "erase_all()" in factory_reset and "CommissioningNvsStore" in factory_reset:
    errors.append("factory reset must not erase immutable commissioning hardware verification")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("Factory reset namespace coverage audit PASS")

# The physical RST/EN + RGB behavior is release-critical and must gate BIN
# generation together with namespace coverage.
rst_rgb = subprocess.run(
    [sys.executable, str(ROOT / "tools" / "check_reset_rgb_contract.py")],
    cwd=ROOT,
    check=False,
)
if rst_rgb.returncode != 0:
    sys.exit(rst_rgb.returncode)
