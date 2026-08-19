from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
ESP = ROOT / "firmware" / "esp-idf"
MAIN = ESP / "main"

errors = []

required_by_include = {
    "esp_http_server.h": "esp_http_server",
    "esp_https_server.h": "esp_https_server",
    "esp_timer.h": "esp_timer",
    "esp_eth.h": "esp_eth",
    "esp_netif.h": "esp_netif",
    "nvs_flash.h": "nvs_flash",
    "sdmmc_cmd.h": "sdmmc",
    "driver/uart.h": "driver",
    "driver/i2c_master.h": "driver",
}

component_dirs = [MAIN]
components_root = ESP / "components"
if components_root.exists():
    component_dirs.extend(
        path for path in sorted(components_root.iterdir())
        if path.is_dir() and (path / "CMakeLists.txt").exists()
    )

for component_dir in component_dirs:
    cmake = component_dir / "CMakeLists.txt"
    if not cmake.exists():
        errors.append(f"missing component CMakeLists.txt: {component_dir.relative_to(ROOT)}")
        continue

    text = cmake.read_text(encoding="utf-8")
    source_refs = re.findall(r'"([^"]+\.(?:cpp|c))"', text)
    for ref in source_refs:
        path = (component_dir / ref).resolve()
        if not path.exists():
            errors.append(
                f"{component_dir.relative_to(ROOT)}: missing CMake source: {ref}"
            )

    source_files = [
        path for path in component_dir.rglob("*")
        if path.is_file() and path.suffix in {".cpp", ".hpp", ".c", ".h"}
    ]
    all_source_text = "\n".join(
        path.read_text(encoding="utf-8", errors="replace")
        for path in source_files
    )

    for header, dependency in required_by_include.items():
        if header in all_source_text and not re.search(
            rf"\b{re.escape(dependency)}\b", text
        ):
            errors.append(
                f"{component_dir.relative_to(ROOT)}: component dependency "
                f"'{dependency}' missing for {header}"
            )

# Cement the telemetry ticket lifetime dependency specifically because this
# component lives outside main and was previously invisible to this audit.
ws_dir = components_root / "websocket_telemetry"
if ws_dir.exists():
    ws_cmake = (ws_dir / "CMakeLists.txt").read_text(encoding="utf-8")
    ws_cpp = (ws_dir / "websocket_telemetry.cpp").read_text(encoding="utf-8")
    if "esp_timer_get_time" in ws_cpp and "esp_timer" not in ws_cmake:
        errors.append(
            "websocket_telemetry: esp_timer_get_time requires esp_timer component dependency"
        )

if errors:
    print("\n".join(f"ERROR: {item}" for item in errors))
    sys.exit(1)

print(f"ESP-IDF component dependency audit PASS ({len(component_dirs)} components)")
