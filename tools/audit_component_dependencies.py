from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
ESP = ROOT / "firmware" / "esp-idf"
MAIN = ESP / "main"
CMAKE = MAIN / "CMakeLists.txt"
CORE_COMPONENT = ESP / "components" / "homeguard_core" / "CMakeLists.txt"
WS_HEADER = ESP / "components" / "websocket_telemetry" / "include" / "websocket_telemetry.hpp"

text = CMAKE.read_text(encoding="utf-8")
core_component = CORE_COMPONENT.read_text(encoding="utf-8")
ws_header = WS_HEADER.read_text(encoding="utf-8")
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

# The ESP dependency graph must not reintroduce the old logical command/
# controller state machines behind main/CMakeLists through homeguard_core.
# WebSocket telemetry needs only token verification + telemetry serialization.
if '"../../../src/telemetry_transport.cpp"' not in core_component:
    errors.append("homeguard_core missing telemetry_transport.cpp")
for legacy in (
    '"../../../src/local_api.cpp"',
    '"../../../src/controller.cpp"',
    '"../../../src/device_command_router.cpp"',
    '"../../../src/device_api_model.cpp"',
):
    if legacy in core_component:
        errors.append(f"legacy command path leaked into ESP homeguard_core: {legacy}")
if '#include "homeguard/telemetry_transport.hpp"' not in ws_header:
    errors.append("websocket telemetry does not use transport-only API")
if '#include "homeguard/local_api.hpp"' in ws_header:
    errors.append("websocket telemetry still depends on legacy local API")

if errors:
    print("\n".join(f"ERROR: {item}" for item in errors))
    sys.exit(1)

print("ESP-IDF component dependency audit PASS")
