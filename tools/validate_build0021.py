from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
ESP = ROOT / "firmware" / "esp-idf"

required = [
    ESP / "CMakeLists.txt",
    ESP / "sdkconfig.defaults",
    ESP / "partitions.csv",
    ESP / "main" / "CMakeLists.txt",
    ESP / "main" / "app_main.cpp",
]

errors = []

for path in required:
    if not path.is_file():
        errors.append(f"missing: {path.relative_to(ROOT)}")

partition_text = (ESP / "partitions.csv").read_text(encoding="utf-8")
for name in ("factory", "ota_0", "ota_1", "storage"):
    if not re.search(rf"^\s*{re.escape(name)}\s*,", partition_text, re.M):
        errors.append(f"partition missing: {name}")

sdk = (ESP / "sdkconfig.defaults").read_text(encoding="utf-8")
for setting in (
    "CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y",
    "CONFIG_SPIRAM_MODE_OCT=y",
    "CONFIG_PARTITION_TABLE_CUSTOM=y",
):
    if setting not in sdk:
        errors.append(f"sdkconfig setting missing: {setting}")

main_cmake = (ESP / "main" / "CMakeLists.txt").read_text(encoding="utf-8")
for source in (
    "app_main.cpp",
    "hg_hardware_bootstrap.cpp",
    "hg_w5500.cpp",
    "hg_onewire_runtime.cpp",
):
    if source not in main_cmake:
        errors.append(f"main source missing from CMake: {source}")

if errors:
    print("\n".join(errors))
    sys.exit(1)

print("Build-0021 structural validation PASS")
