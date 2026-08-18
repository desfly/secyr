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

for asset in ("web/index.html", "web/app.css", "web/app.js", "web/bruce.jpg"):
    require(asset in cmake, f"CMake does not embed/copy {asset}")
for route in ('"/"', '"/index.html"', '"/app.css"', '"/app.js"', '"/bruce.jpg"'):
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

# Navigation regression gate. Historically several menu pairs shared one hash
# target, and routeFromHash() therefore assigned the blue .active class to both
# matching anchors. Every visible navigation item now owns a unique hash route;
# paired items may still scroll to nearby views, but never share the route key.
nav_match = re.search(r"<nav>(.*?)</nav>", html, re.S)
nav_html = nav_match.group(1) if nav_match else ""
require(bool(nav_match), "sidebar navigation block missing")
nav_routes = re.findall(r'<a\b[^>]*\bhref=["\'](#[^"\']+)["\'][^>]*>', nav_html, re.S)
expected_nav_routes = (
    "#overview", "#zones-section", "#zones", "#io-section", "#ioState",
    "#events", "#history", "#networkPage", "#system",
)
require(tuple(nav_routes) == expected_nav_routes,
        f"navigation hash routes changed or reordered: {nav_routes}")
require(len(nav_routes) == len(set(nav_routes)),
        f"duplicate navigation href detected; this can create double blue active state: {nav_routes}")
for route in nav_routes:
    matches = sum(candidate == route for candidate in nav_routes)
    require(matches == 1, f"hash transition {route} would activate {matches} menu items")
    target = route[1:]
    require(re.search(rf'\bid=["\']{re.escape(target)}["\']', html) is not None,
            f"navigation route {route} has no DOM target id={target}")
require(re.search(r'<div\s+class=["\']nav-group-label["\'][^>]*>.*?<span>Налаштування</span>.*?</div>', nav_html, re.S) is not None,
        "Налаштування must remain a non-clickable navigation group label")
nav_anchor_bodies = re.findall(r'<a\b[^>]*>(.*?)</a>', nav_html, re.S)
nav_anchor_labels = [re.sub(r'<[^>]+>', '', body).strip() for body in nav_anchor_bodies]
require(all("Налаштування" not in label for label in nav_anchor_labels),
        "Налаштування regressed into an active-capable navigation link")
require(len(re.findall(r'class=["\'][^"\']*\bactive\b[^"\']*["\']', nav_html)) == 1,
        "HTML must boot with exactly one active navigation item")
require('class="active" href="#overview"' in nav_html,
        "overview must be the single initial active navigation item")
require(js.count('classList.toggle("active"') == 1,
        "active navigation class must have one authoritative JS assignment site")
require('link.getAttribute("href") === hash' in js,
        "routeFromHash must select active navigation by exact hash equality")
require("hashchange" in js and "routeFromHash" in js,
        "hash transitions are not wired to the navigation state resolver")
require("nav a.active{" in css,
        "blue selected-state CSS rule missing")
require("nav a:hover{background" not in css and "nav a:focus{background" not in css and "nav a:focus-visible{background" not in css,
        "hover/focus must not paint a second blue navigation background")

security_commands = (
    "security.arm_away", "security.disarm", "security.arm_home", "security.panic",
)
for command in security_commands:
    require(f'data-command="{command}"' in html, f"quick command button missing: {command}")

for needle in (
    '#wifiScan', '#wifiConnect', '[data-command]', '[data-output-id]', '.sidebar nav a',
    'hashchange', 'routeFromHash', '#networkPage', '/api/v1/network/status',
    '/api/v1/network/scan', '/api/v1/network/connect', '/api/v1/system/security-command',
    '/api/v1/system/output-command', '/api/v1/access/users',
    'document.documentElement.dataset.homeguardUi = "ready"',
):
    require(needle in js, f"app.js contract missing: {needle}")

require("sendSecurityCommand" in js and "JSON.stringify({ command, actor, credential })" in js,
        "security buttons are not posting command + operator credentials")
require('"/api/v1/system/security-command"' in system_http,
        "security-command firmware route missing")
require(re.search(r'authorize\s*\(actor,\s*credential,\s*command\)', system_http) is not None,
        "security-command route does not authorize the requested command")

require('data-output-id="${id}"' in js and "sendOutputCommand(button)" in js,
        "live valve controls are not bound to sendOutputCommand")
require("JSON.stringify({ outputId, active, actor, credential })" in js,
        "valve buttons are not posting output state + operator credentials")
require('"/api/v1/system/output-command"' in output_http,
        "output-command firmware route missing")
require("g_output_http.set_access_control(&g_access_control)" in app_main,
        "output access-control injection missing from app_main")

for command in ("security.arm_home", "security.arm_away", "security.disarm", "valve.open", "valve.close"):
    require(command in access_core, f"User role permission missing: {command}")
require("if (role == AccessRole::Admin) return true" in access_core,
        "Admin is not unrestricted in access policy")
require("if (role == AccessRole::Guest) return false" in access_core,
        "Guest is not fail-closed for control commands")

# First-Admin bootstrap must break the fresh-device deadlock without becoming
# an unauthenticated account-reset path after storage corruption.
for needle in ("accessBootstrap", 'action: "bootstrap"', "bootstrapAccessAdmin"):
    require(needle in js, f"first-Admin Web UI bootstrap missing: {needle}")
require('action == "bootstrap"' in access_http, "firmware first-Admin bootstrap action missing")
require("bootstrap_allowed_" in access_http,
        "AccessHttp does not keep an explicit one-time bootstrap gate")
require("if (!bootstrap_allowed_)" in access_http,
        "bootstrap endpoint is not protected by the factory-fresh gate")
require("access_->user_count() != 0U" in access_http,
        "bootstrap is not locked out after the first user exists")
require("AccessRole::Admin" in access_http,
        "bootstrap does not force the first account to Admin")
require("access_->clear_users()" in access_http,
        "failed bootstrap persistence does not roll back the RAM user")
require("bootstrap_allowed_ = false" in access_http,
        "successful bootstrap does not close the one-time gate")
require("bootstrap_allowed_ = true" in access_http,
        "failed factory bootstrap cannot be retried safely")
require("bool g_access_bootstrap_allowed = false" in app_main,
        "boot code does not default bootstrap to disabled")
require(re.search(r'if\s*\(error\s*==\s*ESP_ERR_NVS_NOT_FOUND\)\s*\{[^}]*g_access_bootstrap_allowed\s*=\s*true', app_main, re.S) is not None,
        "factory-fresh NVS does not explicitly enable first-Admin bootstrap")
require(re.search(r'if\s*\(error\s*!=\s*ESP_OK\)\s*\{[^}]*clear_users\(\)[^}]*bootstrap stays disabled', app_main, re.S) is not None,
        "corrupt access storage does not remain fail-closed with bootstrap disabled")
require("&g_access_store, g_access_bootstrap_allowed" in app_main,
        "factory-fresh bootstrap state is not injected into AccessHttp")

for endpoint in ("/api/v1/network/status", "/api/v1/network/scan", "/api/v1/network/connect"):
    require(endpoint in network, f"network firmware endpoint missing: {endpoint}")
require("networkActor" in js and "networkCredential" in js,
        "Wi-Fi page does not request administrator credentials")
require("JSON.stringify({ ssid, password, actor, credential })" in js,
        "Wi-Fi connect does not send administrator credentials")
require(re.search(r'authorize\s*\(actor,\s*credential,\s*"network\.configure"\)', network) is not None,
        "Wi-Fi configuration is not protected by access control")
require("g_network_http.set_access_control(&g_access_control)" in app_main,
        "NetworkHttp access-control injection missing from app_main")
require("rssi" in network, "Wi-Fi scan/status backend does not expose RSSI")
require("ip" in network, "Wi-Fi status backend does not expose IP")
require("save_credentials" in network and "load_credentials" in network,
        "Wi-Fi reconnect persistence missing")

require((WEB / "bruce.jpg").is_file(), "Bruce JPEG asset missing")
require((WEB / "bruce.jpg").stat().st_size > 1024 if (WEB / "bruce.jpg").exists() else False,
        "Bruce JPEG asset is unexpectedly small")
require('class="bruce"' in html, "Bruce DOM container missing")
require('<img id="bruceArt"' in html, "Bruce portrait image element missing")
require('/bruce.jpg?v=' in html, "Bruce portrait cache-busted URL missing")
require('<svg id="bruceArt"' not in html, "old drawn Bruce SVG placeholder still present")
require(".bruce img{" in html, "Bruce portrait sizing rule missing")
require("EMBED_FILES" in cmake and '"web/bruce.jpg"' in cmake,
        "Bruce JPEG is not embedded as a binary ESP-IDF asset")
require('"image/jpeg"' in web_http, "Bruce route does not use image/jpeg")
require("bruce_jpg_start" in web_http and "bruce_jpg_end" in web_http,
        "Bruce embedded linker symbols missing")

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
print(f" - navigation hash transitions checked: {len(nav_routes)}; single-active invariant enforced")
print(" - quick security buttons -> authorized firmware route: 4")
print(" - live valve buttons -> authorized firmware route: present")
print(" - first Admin bootstrap: factory-fresh NVS only; corruption stays fail-closed")
print(" - Wi-Fi configuration: Admin-authorized; status/scan + RSSI/IP/persistence present")
print(" - roles: Admin full; User arm/disarm + valves; Guest control denied")
print(" - Bruce: approved JPEG embedded and served as a firmware asset")
print(" - embedded assets + anti-cache headers: present")
print(" - runtime build number sourced from GitHub Actions run number")