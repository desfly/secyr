#!/usr/bin/env python3
"""Static release gate for the embedded HomeGuard-S3 Web UI.

This intentionally checks the exact integration points that can make a visually
loaded page non-interactive on the controller: embedded assets, HTTP cache
headers, required DOM controls, JS bindings/routes, Wi-Fi API wiring, and the
Bruce image payload.
"""
from __future__ import annotations

import base64
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
    except Exception as exc:  # pragma: no cover - CI diagnostic path
        errors.append(f"cannot read {path.relative_to(ROOT)}: {exc}")
        return ""


html = text(WEB / "index.html")
css = text(WEB / "app.css")
js = text(WEB / "app.js")
cmake = text(MAIN / "CMakeLists.txt")
http = text(MAIN / "hg_web_http.cpp")
network = text(MAIN / "hg_network_http.cpp")
app_main = text(MAIN / "app_main.cpp")

# Firmware embedding and cache correctness.
for asset in ("web/index.html", "web/app.css", "web/app.js"):
    require(asset in cmake, f"CMake does not embed/copy {asset}")
for route in ('"/"', '"/index.html"', '"/app.css"', '"/app.js"'):
    require(route in http, f"WebHttp route missing: {route}")
for header in ("Cache-Control", "no-store", "Pragma", "Expires"):
    require(header in http, f"WebHttp cache prevention missing: {header}")
require(re.search(r'/app\.css\?v=[^"\']+', html) is not None, "index.html CSS cache-buster missing")
require(re.search(r'/app\.js\?v=[^"\']+', html) is not None, "index.html JS cache-buster missing")

# Controls previously observed dead on hardware.
required_ids = {
    "networkNav", "networkCard", "networkPage", "wifiScan", "wifiNetworks",
    "wifiSsid", "wifiPassword", "wifiConnect", "networkState", "networkSsid",
    "networkIp", "wifiResult", "operatorId", "operatorPin", "refresh", "toast",
}
html_ids = set(re.findall(r'\bid=["\']([^"\']+)["\']', html))
for item in sorted(required_ids):
    require(item in html_ids, f"required DOM id missing: {item}")
for command in ("security.arm_away", "security.disarm", "security.arm_home", "security.panic"):
    require(f'data-command="{command}"' in html, f"quick command button missing: {command}")

# JavaScript must wire all interactive families and deterministic hash routing.
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

# Bruce must be a real, decodable WebP embedded in the CSS used by firmware.
require('class="bruce"' in html, "Bruce DOM element missing")
match = re.search(r'data:image/webp;base64,([A-Za-z0-9+/=]+)', css)
if match is None:
    errors.append("Bruce WebP data URI missing from app.css")
else:
    try:
        payload = base64.b64decode(match.group(1), validate=True)
        require(len(payload) >= 1024, f"Bruce WebP payload suspiciously small: {len(payload)} bytes")
        require(payload[:4] == b"RIFF" and payload[8:12] == b"WEBP", "Bruce payload is not a valid WebP container")
    except Exception as exc:
        errors.append(f"Bruce WebP base64 is invalid: {exc}")
require(".bruce{" in css, "Bruce CSS rule missing")

# Prevent the exact stale build label seen on hardware from returning.
require("Build-0039" not in app_main, "stale Build-0039 runtime label remains in app_main.cpp")

if errors:
    print("Web UI contract FAIL")
    for err in errors:
        print(f" - {err}")
    sys.exit(1)

print("Web UI contract PASS")
print(f" - DOM ids checked: {len(required_ids)}")
print(" - quick security controls: 4")
print(" - Wi-Fi status/scan/connect + RSSI/IP/persistence: present")
print(f" - Bruce WebP payload: {len(payload)} bytes, valid RIFF/WEBP")
print(" - embedded assets + anti-cache headers: present")
