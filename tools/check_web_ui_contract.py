#!/usr/bin/env python3
"""Static release gate for the embedded HomeGuard-S3 Web UI."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / "web"
MAIN = ROOT / "firmware" / "esp-idf" / "main"
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


html = text(WEB / "index.html")
css = text(WEB / "app.css")
js = text(WEB / "app.js")
cmake = text(MAIN / "CMakeLists.txt")
http = text(MAIN / "hg_web_http.cpp")
network = text(MAIN / "hg_network_http.cpp")
app_main = text(MAIN / "app_main.cpp")
build_info = text(MAIN / "hg_build_info.cpp")

for asset in ("web/index.html", "web/app.css", "web/app.js"):
    require(asset in cmake, f"CMake does not embed/copy {asset}")
for route in ('"/"', '"/index.html"', '"/app.css"', '"/app.js"'):
    require(route in http, f"WebHttp route missing: {route}")
for header in ("Cache-Control", "no-store", "Pragma", "Expires"):
    require(header in http, f"WebHttp cache prevention missing: {header}")
require(re.search(r'/app\.css\?v=[^"\']+', html) is not None, "index.html CSS cache-buster missing")
require(re.search(r'/app\.js\?v=[^"\']+', html) is not None, "index.html JS cache-buster missing")

required_ids = {
    "networkNav", "networkCard", "networkPage", "wifiScan", "wifiNetworks",
    "wifiSsid", "wifiPassword", "wifiConnect", "networkState", "networkSsid",
    "networkIp", "wifiResult", "operatorId", "operatorPin", "refresh", "toast",
    "bruceArt",
}
html_ids = set(re.findall(r'\bid=["\']([^"\']+)["\']', html))
for item in sorted(required_ids):
    require(item in html_ids, f"required DOM id missing: {item}")
for command in ("security.arm_away", "security.disarm", "security.arm_home", "security.panic"):
    require(f'data-command="{command}"' in html, f"quick command button missing: {command}")

for needle in (
    '#wifiScan', '#wifiConnect', '[data-command]', '[data-output-id]', '.sidebar nav a',
    'hashchange', 'routeFromHash', '#networkPage', '/api/v1/network/status',
    '/api/v1/network/scan', '/api/v1/network/connect', '/api/v1/system/security-command',
    '/api/v1/system/output-command', 'document.documentElement.dataset.homeguardUi = "ready"',
):
    require(needle in js, f"app.js contract missing: {needle}")
for endpoint in ("/api/v1/network/status", "/api/v1/network/scan", "/api/v1/network/connect"):
    require(endpoint in network, f"network firmware endpoint missing: {endpoint}")
require("rssi" in network, "Wi-Fi scan/status backend does not expose RSSI")
require("ip" in network, "Wi-Fi status backend does not expose IP")
require("save_credentials" in network and "load_credentials" in network, "Wi-Fi reconnect persistence missing")

# Bruce is now inline SVG so it is part of index.html itself and cannot be lost
# due to a missing file, corrupt base64 payload, cache mismatch, or extra route.
require('class="bruce"' in html, "Bruce DOM container missing")
require('<svg id="bruceArt"' in html, "self-contained Bruce SVG missing")
require('viewBox="0 0 220 210"' in html, "Bruce SVG viewport missing")
require(".bruce{background:none!important" in html, "legacy broken Bruce background is not overridden")
require("<ellipse" in html and "<path" in html, "Bruce SVG artwork is unexpectedly empty")

require("Build-0039" not in app_main, "stale Build-0039 runtime label remains in app_main.cpp")
require("HG_CI_BUILD_NUMBER" in build_info, "build endpoint is not using CI build number")
require("HG_CI_BUILD_NUMBER" in cmake and "GITHUB_RUN_NUMBER" in cmake, "CI run number is not compiled into firmware")

if errors:
    print("Web UI contract FAIL")
    for err in errors:
        print(f" - {err}")
    sys.exit(1)

print("Web UI contract PASS")
print(f" - DOM ids checked: {len(required_ids)}")
print(" - quick security controls: 4")
print(" - Wi-Fi status/scan/connect + RSSI/IP/persistence: present")
print(" - Bruce: self-contained inline SVG, no extra firmware asset required")
print(" - embedded assets + anti-cache headers: present")
print(" - runtime build number sourced from GitHub Actions run number")
