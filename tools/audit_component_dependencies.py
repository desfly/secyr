from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
ESP = ROOT / "firmware" / "esp-idf"
MAIN = ESP / "main"
CMAKE = MAIN / "CMakeLists.txt"

text = CMAKE.read_text(encoding="utf-8")
errors = []

source_refs = re.findall(r'"([^"]+\.(?:cpp|c))"', text)
for ref in source_refs:
    path = (MAIN / ref).resolve()
    if not path.exists():
        errors.append(f"missing CMake source: {ref}")

required_by_include = {
    "esp_http_server.h": "esp_http_server",
    "esp_eth.h": "esp_eth",
    "esp_netif.h": "esp_netif",
    "nvs_flash.h": "nvs_flash",
    "sdmmc_cmd.h": "sdmmc",
    "driver/uart.h": "driver",
    "driver/i2c_master.h": "driver",
}

all_source_text = "\n".join(
    p.read_text(encoding="utf-8")
    for p in MAIN.glob("*")
    if p.suffix in {".cpp", ".hpp", ".c", ".h"}
)

for header, component in required_by_include.items():
    if header in all_source_text and component not in text:
        errors.append(
            f"component dependency '{component}' missing for {header}"
        )

if errors:
    print("\n".join(f"ERROR: {item}" for item in errors))
    sys.exit(1)

print("ESP-IDF component dependency audit PASS")
