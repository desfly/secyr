#!/usr/bin/env python3
"""Regression gate for physical-zone alarm propagation into the embedded Web UI."""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "esp-idf" / "main"
WEB = ROOT / "web"
errors = []

def require(condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)

telemetry = (MAIN / "hg_telemetry_runtime.cpp").read_text(encoding="utf-8")
web = (WEB / "app.js").read_text(encoding="utf-8")
web_index = (WEB / "index.html").read_text(encoding="utf-8")
cmake = (MAIN / "CMakeLists.txt").read_text(encoding="utf-8")
web_http = (MAIN / "hg_web_http.cpp").read_text(encoding="utf-8")

# Firmware: an armed physical zone transition must be promoted to partition ALARM
# and dispatched immediately to event/WebSocket consumers.
for needle in (
    "void TelemetryRuntime::update_zone_model(",
    "partition->arm_state == hg::PartitionArmState::Stay",
    "partition->arm_state == hg::PartitionArmState::Away",
    "model_state = armed ? hg::ModelZoneState::Alarm : hg::ModelZoneState::Open",
    "alarm_triggered = alarm_triggered || armed",
    "system_model_->set_partition_arm(1, hg::PartitionArmState::Alarm, now_ms)",
    "system_bus_->dispatch_all()",
):
    require(needle in telemetry, f"physical alarm propagation missing: {needle}")

require("constexpr std::size_t kActiveSecurityZones = 4;" in telemetry,
        "zones 5-8 must remain excluded until commissioned")
require("sample_zone_adc(hardware_->telemetry_adc(), 4, zones);" not in telemetry,
        "telemetry ADS1115 must not be interpreted as zones 5-8")

require('"hg_telemetry_runtime.cpp"' in cmake,
        "ESP-IDF build no longer compiles hg_telemetry_runtime.cpp")
require('file(READ "${CMAKE_CURRENT_LIST_DIR}/../../../web/app.js" HG_WEB_APP_JS)' in cmake,
        "ESP-IDF build no longer embeds the canonical web/app.js")

# Web: armed state plus a new zone alarm event must activate the visible alarm;
# disarm must clear it.
for needle in (
    "const zoneAlarmRuntime = {",
    "function setZoneAlarmActive(active)",
    "function isZoneAlarmEvent(item)",
    'armState === "stay" || armState === "away" || armState === "alarm"',
    "else setZoneAlarmActive(false);",
    'event === "partition.armed" && value === 3',
    "sourceId >= 1 && sourceId <= 4",
    'event === "alarm" || event === "zone.open" || event === "tamper"',
    'if (armState === "alarm") setZoneAlarmActive(true)',
    'securitySummary.textContent = alarm ? "Зафіксовано тривогу" : "Система в нормі"',
    "latestSequence > zoneAlarmRuntime.lastAlarmSequence",
    "setZoneAlarmActive(true)",
    'document.body.classList.remove("hg-zone-alarm")',
    'securityCard.classList.toggle("hg-security-alarm", zoneAlarmRuntime.active)',
    '#securityCard.hg-security-alarm',
    "primeZoneAlarmAudio",
    "const CORE_POLL_MS = 1000;",
):
    require(needle in web, f"Web alarm reaction missing: {needle}")

require('document.querySelector("#securitySummary")' in web,
        "Alarm summary must use stable explicit node")
require('id="securitySummary"' in web_index,
        "Security summary DOM id missing")
require('HOMEGUARD_WEB_ASSET_REV = "alarm-ui-20260929-r2"' in web_http,
        "served app.js revision stamp missing")
require('securityCard.classList.toggle("hg-security-alarm", alarm)' in web,
        "partition renderer must atomically own alarm card presentation")

if errors:
    print("Physical zone -> Web alarm regression gate FAIL")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)

print("Physical zone -> Web alarm regression gate PASS")
print(" - armed physical Open/Short/Tamper can promote partition to ALARM")
print(" - event bus dispatch remains immediate")
print(" - canonical Web UI is embedded in firmware")
print(" - Web security card flashes red with audio on alarm and clears on disarm")
