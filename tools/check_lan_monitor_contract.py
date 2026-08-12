#!/usr/bin/env python3
"""Release gate for HomeGuard-S3 LAN discovery and Web UI monitoring."""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / "web"
MAIN = ROOT / "firmware" / "esp-idf" / "main"
errors: list[str] = []


def read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except Exception as exc:
        errors.append(f"cannot read {path.relative_to(ROOT)}: {exc}")
        return ""


def require(condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


html = read(WEB / "index.html")
js = read(WEB / "lan-monitor.js")
cmake = read(MAIN / "CMakeLists.txt")
web_http = read(MAIN / "hg_web_http.cpp")
web_hpp = read(MAIN / "hg_web_http.hpp")
lan_http = read(MAIN / "hg_lan_discovery_http.cpp")
app_main = read(MAIN / "app_main.cpp")

for dom_id in ("lanRefresh", "lanScanState", "lanDeviceCount", "lanDevices"):
    require(f'id="{dom_id}"' in html, f"LAN monitor DOM id missing: {dom_id}")
require('/lan-monitor.js?v=' in html, "LAN monitor script is not referenced by index.html")
require("Пристрої домашньої мережі" in html, "LAN monitor panel heading missing")

for needle in (
    '/api/v1/network/lan-scan',
    'INTERVAL_MS = 15000',
    '#lanRefresh',
    '#lanDevices',
    '#lanDeviceCount',
    '#lanScanState',
    'networkPageVisible()',
    'setInterval',
    'cache: "no-store"',
    'escapeHtml',
    'window.HomeGuardLanMonitor',
):
    require(needle in js, f"LAN monitor JS contract missing: {needle}")

require('"/api/v1/network/lan-scan"' in lan_http, "firmware LAN scan endpoint missing")
require("etharp_get_entry" in lan_http, "LAN scan does not enumerate lwIP ARP table")
require("ARP_TABLE_SIZE" in lan_http, "LAN scan does not use ESP-IDF/lwIP ARP_TABLE_SIZE")
require("\"online\":true" in lan_http, "LAN scan response does not expose online state")
require("\"hostname\"" in lan_http, "LAN scan response does not expose hostname field")
require("g_lan_discovery_http.register_handlers" in app_main, "LAN discovery handlers are not registered in app_main")

require('configure_file("${CMAKE_CURRENT_LIST_DIR}/../../../web/lan-monitor.js"' in cmake,
        "CMake does not copy lan-monitor.js into the ESP-IDF component")
require('"web/lan-monitor.js"' in cmake, "lan-monitor.js is not embedded into firmware")
require('"/lan-monitor.js"' in web_http, "WebHttp route missing: /lan-monitor.js")
require("lan_monitor_js_start" in web_http and "lan_monitor_js_end" in web_http,
        "embedded lan-monitor.js linker symbols missing")
require("lan_monitor_js_get" in web_http and "lan_monitor_js_get" in web_hpp,
        "WebHttp LAN monitor handler declaration/definition missing")
require('"application/javascript; charset=utf-8"' in web_http,
        "WebHttp JavaScript MIME type missing")

if errors:
    print("LAN monitor contract FAIL")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)

print("LAN monitor contract PASS")
print(" - firmware endpoint: /api/v1/network/lan-scan")
print(" - lwIP ARP enumeration: present")
print(" - Web UI table + manual refresh: present")
print(" - visible-page auto refresh: 15 seconds")
print(" - HTML escaping: present")
print(" - firmware embed + HTTP asset route: present")
