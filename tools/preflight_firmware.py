from pathlib import Path
import csv
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
ESP = ROOT / "firmware" / "esp-idf"
MAIN = ESP / "main"
WORKFLOW = ROOT / ".github" / "workflows" / "homeguard-build.yml"
SETUP_AP_GUARD = MAIN / "hg_setup_ap_guard.cpp"

errors = []
warnings = []

required = [
    ESP / "CMakeLists.txt",
    ESP / "sdkconfig.defaults",
    ESP / "partitions.csv",
    MAIN / "CMakeLists.txt",
    MAIN / "app_main.cpp",
    MAIN / "hg_version.hpp",
    SETUP_AP_GUARD,
    WORKFLOW,
]

for path in required:
    if not path.exists():
        errors.append(f"missing: {path.relative_to(ROOT)}")

partition_rows = []
with (ESP / "partitions.csv").open(encoding="utf-8") as f:
    for row in csv.reader(line for line in f if not line.lstrip().startswith("#")):
        if row and any(cell.strip() for cell in row):
            partition_rows.append([cell.strip() for cell in row])

names = [row[0] for row in partition_rows]
for name in ["nvs", "otadata", "factory", "ota_0", "ota_1", "storage"]:
    if name not in names:
        errors.append(f"partition missing: {name}")

limit = 16 * 1024 * 1024
for row in partition_rows:
    if len(row) < 5:
        errors.append(f"invalid partition row: {row}")
        continue
    try:
        offset = int(row[3], 0)
        size = int(row[4], 0)
        if offset + size > limit:
            errors.append(f"partition exceeds 16 MiB: {row[0]}")
    except ValueError:
        warnings.append(f"non-numeric partition value: {row[0]}")

sdk = (ESP / "sdkconfig.defaults").read_text(encoding="utf-8")
required_sdk = [
    "CONFIG_IDF_TARGET=\"esp32s3\"",
    "CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y",
    "CONFIG_SPIRAM_MODE_OCT=y",
    "CONFIG_PARTITION_TABLE_CUSTOM=y",
]
for item in required_sdk:
    if item not in sdk:
        errors.append(f"sdkconfig missing: {item}")

socket_match = re.search(r"^CONFIG_LWIP_MAX_SOCKETS=(\d+)$", sdk, flags=re.MULTILINE)
if not socket_match:
    errors.append("sdkconfig missing explicit CONFIG_LWIP_MAX_SOCKETS")
elif int(socket_match.group(1)) < 16:
    errors.append("CONFIG_LWIP_MAX_SOCKETS must be at least 16 for HTTPD + browser + discovery/MQTT")

app_main_text = (MAIN / "app_main.cpp").read_text(encoding="utf-8")
if "config.lru_purge_enable = true;" not in app_main_text:
    errors.append("HTTP server must keep LRU socket purge enabled")

# Recovery invariant: creating an Admin is not proof that the station handover
# succeeded. The setup AP may be disabled only after WIFI_STA_DEF owns IPv4.
# This source gate prevents a future regression back to Admin -> immediate AP off.
if SETUP_AP_GUARD.exists():
    setup_ap_guard_text = SETUP_AP_GUARD.read_text(encoding="utf-8")
    required_handover_contract = [
        "bool sta_has_ipv4()",
        'esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")',
        "esp_netif_get_ip_info(sta_netif, &info) == ESP_OK && info.ip.addr != 0U",
        "if (!sta_has_ipv4()) return ESP_ERR_INVALID_STATE;",
        "Admin ready but STA has no IPv4; keeping setup AP active until network handover succeeds",
        "STA IPv4 ready; closing setup AP",
    ]
    for item in required_handover_contract:
        if item not in setup_ap_guard_text:
            errors.append(f"setup AP handover contract missing: {item}")

    immediate_shutdown = (
        "if (!access_runtime::bootstrap_allowed()) {\n"
        "        return disable_setup_ap();\n"
        "    }"
    )
    if immediate_shutdown in setup_ap_guard_text:
        errors.append("setup AP must not shut down merely because Admin/bootstrap is locked")

    wait_pos = setup_ap_guard_text.find("if (!sta_has_ipv4()) {")
    close_pos = setup_ap_guard_text.find("STA IPv4 ready; closing setup AP")
    disable_pos = setup_ap_guard_text.find("const auto error = disable_setup_ap_with_retries();")
    if wait_pos < 0 or close_pos < 0 or disable_pos < 0 or not (wait_pos < close_pos < disable_pos):
        errors.append("setup AP shutdown must be ordered after the STA IPv4 readiness gate")

version_text = (MAIN / "hg_version.hpp").read_text(encoding="utf-8")
version_fields = dict(re.findall(r'^#define\s+(HG_[A-Z0-9_]+)\s+"([^"]*)"', version_text, flags=re.MULTILINE))
for field in ("HG_PROJECT_NAME", "HG_BUILD_NUMBER", "HG_FIRMWARE_VERSION", "HG_ESP_IDF_REQUIRED"):
    if not version_fields.get(field):
        errors.append(f"hg_version.hpp missing or empty: {field}")

build_number = version_fields.get("HG_BUILD_NUMBER", "")
if build_number and not re.fullmatch(r"\d{4}", build_number):
    errors.append("HG_BUILD_NUMBER must be exactly four digits")

workflow_text = WORKFLOW.read_text(encoding="utf-8") if WORKFLOW.exists() else ""
idf_required = version_fields.get("HG_ESP_IDF_REQUIRED", "")
if idf_required:
    if f"ESP-IDF {idf_required} firmware" not in workflow_text:
        errors.append("hg_version.hpp ESP-IDF version does not match workflow job name")
    if f"espressif/idf:v{idf_required}" not in workflow_text:
        errors.append("hg_version.hpp ESP-IDF version does not match workflow container")

cmake = (MAIN / "CMakeLists.txt").read_text(encoding="utf-8")
source_refs = re.findall(r'"([^\"]+\.(?:cpp|c))"', cmake)
for ref in source_refs:
    source = (MAIN / ref).resolve()
    if not source.exists():
        errors.append(f"CMake source missing: {ref}")

seen = set()
for ref in source_refs:
    if ref in seen:
        errors.append(f"duplicate CMake source: {ref}")
    seen.add(ref)

for cpp in MAIN.glob("*.cpp"):
    text = cpp.read_text(encoding="utf-8")
    if "ESP_RETURN_ON_ERROR" in text and '#include "esp_check.h"' not in text:
        errors.append(f"esp_check.h missing: {cpp.name}")

for message in warnings:
    print(f"WARNING: {message}")

if errors:
    for message in errors:
        print(f"ERROR: {message}")
    sys.exit(1)

print("Firmware preflight PASS")
