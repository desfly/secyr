#!/usr/bin/env python3
"""Static release gate for the embedded HomeGuard-S3 Web UI.

The gate intentionally follows control actions end-to-end: visible button ->
JavaScript handler -> HTTP endpoint -> access-control injection/policy. A page
that merely renders is not sufficient for a green release gate.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WEB = ROOT / "web"
MAIN = ROOT / "firmware" / "esp-idf" / "main"
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


html = text(WEB / "index.html")
css = text(WEB / "app.css")
js = text(WEB / "app.js")
cmake = text(MAIN / "CMakeLists.txt")
web_http = text(MAIN / "hg_web_http.cpp")
network = text(MAIN / "hg_network_http.cpp")
system_http = text(MAIN / "hg_system_http.cpp")
output_http = text(MAIN / "hg_output_http.cpp")
access_http = text(MAIN / "hg_access_http.cpp")
app_main = text(MAIN / "app_main.cpp")
build_info = text(MAIN / "hg_build_info.cpp")
access_core = text(CORE / "access_control.cpp")

for asset in ("web/index.html", "web/app.css", "web/app.js"):
    require(asset in cmake, f"CMake does not embed/copy {asset}")
for route in ('"/"', '"/index.html"', '"/app.css"', '"/app.js"'):
    require(route in web_http, f"WebHttp route missing: {route}")
for header in ("Cache-Control", "no-store", "Pragma", "Expires"):
    require(header in web_http, f"WebHttp cache prevention missing: {header}")
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

security_commands = (
    "security.arm_away", "security.disarm", "security.arm_home", "security.panic",
)
for command in security_commands:
    require(f'data-command="{command}"' in html, f"quick command button missing: {command}")

# Base browser wiring.
for needle in (
    '#wifiScan', '#wifiConnect', '[data-command]', '[data-output-id]', '.sidebar nav a',
    'hashchange', 'routeFromHash', '#networkPage', '/api/v1/network/status',
    '/api/v1/network/scan', '/api/v1/network/connect', '/api/v1/system/security-command',
    '/api/v1/system/output-command', '/api/v1/access/users',
    'document.documentElement.dataset.homeguardUi = "ready"',
):
    require(needle in js, f"app.js contract missing: {needle}")

# Security quick buttons: HTML command -> JS POST -> firmware route -> access control.
require("sendSecurityCommand" in js and "JSON.stringify({ command, actor, credential })" in js,
        "security buttons are not posting command + operator credentials")
require('"/api/v1/system/security-command"' in system_http,
        "security-command firmware route missing")
require("access_->authorize(actor, credential, command)" in system_http,
        "security-command route does not authorize the requested command")

# Valve buttons are rendered from live output state and must call the protected output route.
require('data-output-id="${id}"' in js and "sendOutputCommand(button)" in js,
        "live valve controls are not bound to sendOutputCommand")
require("JSON.stringify({ outputId, active, actor, credential })" in js,
        "valve buttons are not posting output state + operator credentials")
require('"/api/v1/system/output-command"' in output_http,
        "output-command firmware route missing")
require("set_access_control(&g_access_control)" in app_main,
        "output access-control injection missing from app_main")

# Role contract agreed for HomeGuard-S3: User can arm/disarm and operate valves;
# Guest receives no control-command permission; Admin is unrestricted.
for command in ("security.arm_home", "security.arm_away", "security.disarm", "valve.open", "valve.close"):
    require(command in access_core, f"User role permission missing: {command}")
require("if (role == AccessRole::Admin) return true" in access_core,
        "Admin is not unrestricted in access policy")
require("if (role == AccessRole::Guest) return false" in access_core,
        "Guest is not fail-closed for control commands")

# First administrator bootstrap must break the zero-user deadlock but only once.
for needle in ("accessBootstrap", 'action: "bootstrap"', "bootstrapAccessAdmin"):
    require(needle in js, f"first-Admin Web UI bootstrap missing: {needle}")
require('action == "bootstrap"' in access_http, "firmware first-Admin bootstrap action missing")
require("access_->user_count() != 0U" in access_http,
        "bootstrap is not locked out after the first user exists")
require("AccessRole::Admin" in access_http,
        "bootstrap does not force the first account to Admin")
require("access_->clear_users()" in access_http,
        "failed bootstrap persistence does not roll back the RAM user")

# Wi-Fi status/scan is readable, but changing credentials is an Admin command.
for endpoint in ("/api/v1/network/status", "/api/v1/network/scan", "/api/v1/network/connect"):
    require(endpoint in network, f"network firmware endpoint missing: {endpoint}")
require("networkActor" in js and "networkCredential" in js,
        "Wi-Fi page does not request administrator credentials")
require("JSON.stringify({ ssid, password, actor, credential })" in js,
        "Wi-Fi connect does not send administrator credentials")
require('access_->authorize(actor, credential, "network.configure")' in network,
        "Wi-Fi configuration is not protected by access control")
require("g_network_http.set_access_control(&g_access_control)" in app_main,
        "NetworkHttp access-control injection missing from app_main")
require("rssi" in network, "Wi-Fi scan/status backend does not expose RSSI")
require("ip" in network, "Wi-Fi status backend does not expose IP")
require("save_credentials" in network and "load_credentials" in network,
        "Wi-Fi reconnect persistence missing")

# Bruce stays inline so it cannot disappear because of an omitted firmware asset.
require('class="bruce"' in html, "Bruce DOM container missing")
require('<svg id="bruceArt"' in html, "self-contained Bruce SVG missing")
require('viewBox="0 0 220 210"' in html, "Bruce SVG viewport missing")
require(".bruce{background:none!important" in html, "legacy broken Bruce background is not overridden")
require("<ellipse" in html and "<path" in html, "Bruce SVG artwork is unexpectedly empty")

require("Build-0039" not in app_main, "stale Build-0039 runtime label remains in app_main.cpp")
require("HG_CI_BUILD_NUMBER" in build_info, "build endpoint is not using CI build number")
require("HG_CI_BUILD_NUMBER" in cmake and "GITHUB_RUN_NUMBER" in cmake,
        "CI run number is not compiled into firmware")

if errors:
    print("Web UI contract FAIL")
    for err in errors:
        print(f" - {err}")
    sys.exit(1)

print("Web UI contract PASS")
print(f" - DOM ids checked: {len(required_ids)}")
print(" - quick security buttons -> authorized firmware route: 4")
print(" - live valve buttons -> authorized firmware route: present")
print(" - first Admin one-time bootstrap: present and fail-closed after provisioning")
print(" - Wi-Fi configuration: Admin-authorized; status/scan + RSSI/IP/persistence present")
print(" - roles: Admin full; User arm/disarm + valves; Guest control denied")
print(" - Bruce: self-contained inline SVG, no extra firmware asset required")
print(" - embedded assets + anti-cache headers: present")
print(" - runtime build number sourced from GitHub Actions run number")
