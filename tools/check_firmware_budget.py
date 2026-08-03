from pathlib import Path
import csv
import json
import sys

ROOT = Path(__file__).resolve().parents[1]
PARTITIONS = ROOT / "firmware" / "esp-idf" / "partitions.csv"

partitions = {}
with PARTITIONS.open(encoding="utf-8") as handle:
    rows = csv.reader(
        line for line in handle
        if line.strip() and not line.lstrip().startswith("#")
    )
    for row in rows:
        row = [cell.strip() for cell in row]
        partitions[row[0]] = {
            "type": row[1],
            "subtype": row[2],
            "offset": int(row[3], 0),
            "size": int(row[4], 0),
        }

errors = []
required = ("factory", "ota_0", "ota_1", "storage")
for name in required:
    if name not in partitions:
        errors.append(f"missing partition: {name}")

app_sizes = [
    partitions[name]["size"]
    for name in ("factory", "ota_0", "ota_1")
    if name in partitions
]

if app_sizes and len(set(app_sizes)) != 1:
    errors.append("factory/ota application partition sizes differ")

app_budget = min(app_sizes) if app_sizes else 0
warning_threshold = int(app_budget * 0.85)

report = {
    "flash_size_bytes": 16 * 1024 * 1024,
    "application_partition_bytes": app_budget,
    "warning_threshold_bytes": warning_threshold,
    "storage_partition_bytes": partitions.get("storage", {}).get("size", 0),
    "partitions": partitions,
}

output = ROOT / "firmware-budget.json"
output.write_text(json.dumps(report, indent=2), encoding="utf-8")

if app_budget < 2 * 1024 * 1024:
    errors.append("application partition smaller than 2 MiB")

if errors:
    print("\n".join(f"ERROR: {item}" for item in errors))
    sys.exit(1)

print(
    "Firmware budget PASS: "
    f"app={app_budget} bytes, warn={warning_threshold} bytes"
)
