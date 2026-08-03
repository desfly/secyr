from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / "firmware"

reserved = {0, 3, 19, 20, 35, 36, 37, 43, 44, 45, 46, 48}
allowed_profile = FIRMWARE / "esp-idf" / "main" / "hg_board_hw678.hpp"

errors = []
warnings = []

for path in FIRMWARE.rglob("*"):
    if path.suffix not in {".cpp", ".hpp", ".c", ".h"}:
        continue

    text = path.read_text(encoding="utf-8")
    for match in re.finditer(r"\bGPIO_NUM_(\d+)\b", text):
        gpio = int(match.group(1))
        if gpio in reserved and path != allowed_profile:
            errors.append(
                f"{path.relative_to(ROOT)} references reserved GPIO{gpio}"
            )

# Direct numeric use in board profile must be unique among assigned external pins.
profile = allowed_profile.read_text(encoding="utf-8")
assigned = []
for name, gpio in re.findall(
    r"k([A-Za-z0-9_]+)\s*=\s*GPIO_NUM_(\d+)",
    profile,
):
    assigned.append((name, int(gpio)))

seen = {}
for name, gpio in assigned:
    if gpio in seen:
        errors.append(
            f"GPIO{gpio} assigned twice: {seen[gpio]} and {name}"
        )
    seen[gpio] = name

for name, gpio in assigned:
    if gpio in reserved:
        errors.append(
            f"external function {name} uses reserved GPIO{gpio}"
        )

for warning in warnings:
    print(f"WARNING: {warning}")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("GPIO safety audit PASS")
