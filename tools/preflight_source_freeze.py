#!/usr/bin/env python3
from __future__ import annotations

import csv
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ESP = ROOT / "firmware" / "esp-idf"
MAIN = ESP / "main"
CORE = ROOT / "firmware" / "src"
errors: list[str] = []


def require(condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


def text(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except Exception as exc:
        errors.append(f"cannot read {path.relative_to(ROOT)}: {exc}")
        return ""


required = [
    ESP / "CMakeLists.txt",
    ESP / "sdkconfig.defaults",
    ESP / "partitions.csv",
    MAIN / "CMakeLists.txt",
    MAIN / "app_main.cpp",
    MAIN / "hg_service_http.cpp",
    MAIN / "hg_network_http.cpp",
    MAIN / "hg_output_supervisor.cpp",
    CORE / "physical_output_runtime.cpp",
    CORE / "access_control.cpp",
]
for path in required:
    require(path.is_file(), f"missing required source: {path.relative_to(ROOT)}")

# Flash layout: required partitions, no overlap, never beyond 16 MiB.
rows: list[list[str]] = []
partitions = ESP / "partitions.csv"
if partitions.is_file():
    with partitions.open(encoding="utf-8") as handle:
        rows = [
            [cell.strip() for cell in row]
            for row in csv.reader(line for line in handle if not line.lstrip().startswith("#"))
            if row and any(cell.strip() for cell in row)
        ]
    names = {row[0] for row in rows if row}
    for name in ("nvs", "otadata", "phy_init", "factory", "ota_0", "ota_1", "storage"):
        require(name in names, f"partition missing: {name}")

    numeric: list[tuple[int, int, str]] = []
    for row in rows:
        if len(row) < 5:
            errors.append(f"invalid partition row: {row}")
            continue
        try:
            offset = int(row[3], 0)
            size = int(row[4], 0)
        except ValueError:
            continue
        require(offset >= 0 and size > 0, f"invalid partition range: {row[0]}")
        require(offset + size <= 16 * 1024 * 1024, f"partition exceeds 16 MiB: {row[0]}")
        numeric.append((offset, offset + size, row[0]))
    ordered = sorted(numeric)
    for previous, current in zip(ordered, ordered[1:]):
        require(previous[1] <= current[0], f"partition overlap: {previous[2]} -> {current[2]}")

sdk = text(ESP / "sdkconfig.defaults")
for token in (
    'CONFIG_IDF_TARGET="esp32s3"',
    "CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y",
    "CONFIG_SPIRAM_MODE_OCT=y",
    "CONFIG_PARTITION_TABLE_CUSTOM=y",
):
    require(token in sdk, f"sdkconfig contract missing: {token}")

# Every ESP-IDF source referenced by main/CMakeLists must exist exactly once.
cmake = text(MAIN / "CMakeLists.txt")
source_refs = re.findall(r'"([^\"]+\.(?:cpp|c))"', cmake)
require(len(source_refs) == len(set(source_refs)), "duplicate source entry in ESP-IDF main/CMakeLists.txt")
for ref in source_refs:
    require((MAIN / ref).resolve().is_file(), f"CMake source missing: {ref}")
for cpp in MAIN.glob("*.cpp"):
    body = text(cpp)
    if "ESP_RETURN_ON_ERROR" in body:
        require('#include "esp_check.h"' in body, f"esp_check.h missing: {cpp.name}")

app = text(MAIN / "app_main.cpp")
network = text(MAIN / "hg_network_http.cpp")
service = text(MAIN / "hg_service_http.cpp")
physical = text(CORE / "physical_output_runtime.cpp")
access = text(CORE / "access_control.cpp")

# Wi-Fi: GET status is read-only; reconnect is event/timer driven; candidates
# commit to NVS only after got-IP proves the requested SSID.
for token in ("WIFI_EVENT_STA_DISCONNECTED", "IP_EVENT_STA_GOT_IP", "esp_timer_start_once", "kReconnectMaximumUs"):
    require(token in network, f"Wi-Fi recovery contract missing: {token}")
status_start = network.find("esp_err_t NetworkHttp::handle_status")
status_end = network.find("esp_err_t NetworkHttp::handle_scan", status_start)
require(status_start >= 0 and status_end > status_start, "network status handler slice missing")
if status_start >= 0 and status_end > status_start:
    require("esp_wifi_set_mode" not in network[status_start:status_end], "GET /network/status changes Wi-Fi mode")
scan_start = network.find("std::string NetworkHttp::scan_json")
require(scan_start >= 0, "Wi-Fi scan implementation missing")
if scan_start >= 0:
    require("esp_wifi_disconnect" not in network[scan_start:], "Wi-Fi scan deliberately disconnects STA")
connect_start = network.find("esp_err_t NetworkHttp::handle_connect")
connect_end = network.find("bool NetworkHttp::apply_sta", connect_start)
require(connect_start >= 0 and connect_end > connect_start, "Wi-Fi connect handler slice missing")
if connect_start >= 0 and connect_end > connect_start:
    require("save_credentials(" not in network[connect_start:connect_end], "candidate Wi-Fi persisted before got-IP")
require("save_credentials(pending_ssid_, pending_password_)" in network, "got-IP candidate NVS commit missing")
require("connected_ssid != pending_ssid_" in network, "got-IP SSID verification missing")

# Clean boot must configure every physical channel OFF before deciding whether
# hardware/commissioning is allowed to enable normal operation.
off_pos = physical.find("for (const auto channel : kAllChannels)")
verify_pos = physical.find("hardware_verified_ = hardware_verification_allows_outputs")
block_pos = physical.find("if (!hardware_verified_)", verify_pos)
require(off_pos >= 0 and verify_pos > off_pos and block_pos > verify_pos,
        "physical outputs are not forced OFF before hardware verification gate")
require("return true;" in physical[block_pos:block_pos + 240] if block_pos >= 0 else False,
        "invalid hardware must initialize fail-closed so supervisor can still run")

# Production firmware has exactly one actuator state/ownership path. Legacy
# logical Controller/DeviceApi/LocalApi command machines may remain as host
# library code, but they must never be linked into the ESP32-S3 image.
for forbidden in (
    '"../../src/device_command_router.cpp"',
    '"../../src/device_api_model.cpp"',
    '"../../src/controller.cpp"',
    '"../../src/local_api.cpp"',
):
    require(forbidden not in cmake, f"legacy actuator command path restored to ESP-IDF binary: {forbidden}")
for mandatory in (
    '"hg_mcp23017_output_backend.cpp"',
    '"hg_output_supervisor.cpp"',
    '"../../src/output_interlock.cpp"',
    '"../../src/output_command.cpp"',
    '"../../src/physical_output_runtime.cpp"',
):
    require(mandatory in cmake, f"production actuator owner missing from ESP-IDF binary: {mandatory}")

# HW-678 uses MCP23017 only. Supervisor must start on clean/blocked boot and own
# runtime synchronization independently of HTTP commands.
require("Mcp23017OutputBackend g_mcp_outputs" in app, "MCP23017 backend missing from app_main")
require("GpioOutputBackend g_gpio_outputs" not in app, "legacy direct-GPIO backend restored")
require('"hg_gpio_output_backend.cpp"' not in cmake, "legacy direct-GPIO backend restored to ESP-IDF binary")
require("g_mcp_outputs.attach(&g_hardware.io_expander())" in app, "MCP23017 backend is not attached to hardware runtime")
hardware_pos = app.find("const auto hardware_error = g_hardware.initialize()")
physical_pos = app.find("initialize_physical_outputs();", hardware_pos)
supervisor_pos = app.find("g_output_supervisor.start(", physical_pos)
require(hardware_pos >= 0 and physical_pos > hardware_pos and supervisor_pos > physical_pos,
        "hardware -> physical runtime -> supervisor boot order regressed")
require("&g_physical_outputs,\n            &g_system_model" in app, "OutputSupervisor two-argument wiring regressed")

# Service commissioning is single-path. Sticky destructive lockout always ends
# in reboot, otherwise same-boot recommissioning cannot legally clear the latch.
require(not (MAIN / "hg_commissioning_http.hpp").exists(), "obsolete CommissioningHttp declaration restored")
for route in (
    "/api/v1/service/readiness",
    "/api/v1/service/maintenance",
    "/api/v1/service/commissioning/hardware-verify",
    "/api/v1/service/commissioning/dry-run",
    "/api/v1/service/commissioning/valve-profile",
    "/api/v1/service/commissioning/bench-pulse",
    "/api/v1/service/commissioning/actuator-accept",
    "/api/v1/service/invalidate",
    "/api/v1/service/factory-reset",
):
    require(route in service, f"ServiceHttp route missing: {route}")
require(service.count("lockout_fail_closed()") >= 2, "destructive service routes are not both sticky fail-closed")
require(service.count("esp_restart();") >= 2, "invalidate and factory-reset do not both reboot")
require("return live_mcp_ready(hardware);" in service, "optional modules block actuator dry-run")

# Cross-task authorization is shared by HTTP and MQTT; audit/throttle/user state
# must remain serialized.
require("std::recursive_mutex g_access_control_mutex" in access, "AccessControl cross-task mutex missing")
require(access.count("std::scoped_lock lock(g_access_control_mutex)") >= 12,
        "AccessControl state is not consistently serialized")

# HTTP route registration can compile but fail at runtime when handler capacity is
# exhausted. Keep four spare slots beyond all literal route registrations.
match = re.search(r"config\.max_uri_handlers\s*=\s*(\d+)", app)
require(match is not None, "max_uri_handlers is not explicit")
if match is not None:
    capacity = int(match.group(1))
    literal_routes = 0
    for cpp in MAIN.glob("*.cpp"):
        literal_routes += len(re.findall(r"\.uri\s*=\s*\"[^\"]+\"", text(cpp)))
    require(capacity >= literal_routes + 4,
            f"HTTP URI capacity {capacity} < {literal_routes} routes + 4 spare")
    print(f"HTTP handler budget: routes={literal_routes} capacity={capacity} spare={capacity-literal_routes}")

require("esp_log_set_vprintf(esp_rom_vprintf)" not in app, "non-reentrant ROM logger override restored")

if errors:
    print("Source-freeze preflight FAIL")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)

print("Source-freeze preflight PASS")
