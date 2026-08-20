#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / "web"
MAIN = ROOT / "firmware" / "esp-idf" / "main"

html = (WEB / "index.html").read_text(encoding="utf-8")
js = (WEB / "app.js").read_text(encoding="utf-8")
lan_h = (MAIN / "hg_lan_http.hpp").read_text(encoding="utf-8")
lan_cpp = (MAIN / "hg_lan_http.cpp").read_text(encoding="utf-8")
app_main = (MAIN / "app_main.cpp").read_text(encoding="utf-8")
cmake = (MAIN / "CMakeLists.txt").read_text(encoding="utf-8")

errors = []

def require(condition, message):
    if not condition:
        errors.append(message)

for dom_id in ("lanScan", "lanState", "lanDevices"):
    require(f'id="{dom_id}"' in html, f"LAN DOM id missing: {dom_id}")

for endpoint in ("/api/v1/lan/devices", "/api/v1/lan/scan"):
    require(endpoint in js, f"Web UI does not call {endpoint}")
    require(endpoint in lan_cpp, f"LAN firmware endpoint missing: {endpoint}")

require("refreshLan(false)" in js, "LAN passive refresh missing")
require("refreshLan(true)" in js, "LAN active scan button path missing")
require("LAN_POLL_MS" in js and "schedulerStep" in js,
        "LAN refresh is not owned by the centralized Web UI scheduler")
require("serializedApiFetch" in js and "apiQueueTail" in js,
        "LAN/API traffic is not protected by single-flight HTTP serialization")
require("setInterval(" not in js and "setInterval(" not in html,
        "legacy independent Web UI polling timers returned")
require("MAC" in js and "IP" in js, "LAN UI does not expose IP/MAC")

require("etharp_find_addr" in lan_cpp, "LAN backend does not read ARP cache")
require("stimulate_arp_cache" in lan_cpp and "sendto" in lan_cpp, "LAN active scan does not stimulate ARP discovery")
require("kActiveScanCooldownUs" in lan_cpp and "g_last_active_scan_us" in lan_cpp,
        "LAN active scan lacks cooldown protection")
require("scanThrottled" in lan_cpp,
        "LAN API does not expose whether an active scan was throttled")
require("now_us - g_last_active_scan_us < kActiveScanCooldownUs" in lan_cpp,
        "LAN active scan cooldown is not enforced before /24 stimulus")
require("hg_lan_http.cpp" in cmake, "LAN backend not compiled into ESP-IDF main component")
require('#include "hg_lan_http.hpp"' in app_main, "LanHttp header not wired into app_main")
require("g_lan_http.register_handlers" in app_main, "LanHttp routes not registered")
require("class LanHttp" in lan_h, "LanHttp declaration missing")

if errors:
    print("LAN UI/API contract FAIL")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)

print("LAN UI/API contract PASS")
print(" - passive ARP monitor: centralized scheduler")
print(" - active /24 ARP scan: present + cooldown protected")
print(" - Web UI API transport: serialized single-flight")
print(" - Web UI IP/MAC list + scan button: present")
print(" - firmware registration + build wiring: present")
