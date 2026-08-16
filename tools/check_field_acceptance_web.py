#!/usr/bin/env python3
"""Release gate for field-proven Web UI acceptance rules.

This check intentionally verifies the firmware-served suffix, not only the
source assets in /web. Field Build-948 proved that a feature present only in a
secondary JavaScript asset is not sufficient if the flashed UI never exposes
it.
"""
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "esp-idf" / "main"
web_http = (MAIN / "hg_web_http.cpp").read_text(encoding="utf-8")
system_http = (MAIN / "hg_system_http.cpp").read_text(encoding="utf-8")
access_http = (MAIN / "hg_access_http.cpp").read_text(encoding="utf-8")
web_app = (ROOT / "web" / "app.js").read_text(encoding="utf-8")
errors: list[str] = []


def require(condition: bool, message: str) -> None:
    if not condition:
        errors.append(message)


# Factory Reset must be visible and executable from the firmware-owned UI.
for needle in (
    "firmwareFactoryResetPanel",
    "firmwareFactoryReset",
    "factoryActor",
    "factoryCredential",
    "/api/v1/system/factory-reset",
    "ERASE_ALL",
    "ПОВНЕ СКИДАННЯ",
):
    require(needle in web_http, f"firmware Web Factory Reset contract missing: {needle}")

require(web_http.count("window.confirm(") >= 2,
        "Factory Reset does not require two explicit destructive confirmations")
require('method: "POST"' in web_http,
        "Factory Reset UI is not posting to the controller")
require('body: JSON.stringify({ actor: actorValue, credential: credentialValue, confirm: "ERASE_ALL" })' in web_http,
        "Factory Reset UI does not submit Admin credentials plus ERASE_ALL")

# The first-Admin action is allowed only on a truly factory-fresh controller.
# The browser must use a read-only status endpoint; fake/partial bootstrap POSTs
# are forbidden because they pollute logs and couple UX to error ordering.
for needle in (
    "syncBootstrapAvailability",
    "applyBootstrapVisibility",
    "bootstrapProbeInFlight",
    "bootstrapAvailable",
    'fetch("/api/v1/access/status"',
    'method: "GET"',
    "body.bootstrapAllowed === true",
    "Number(body.userCount || 0) === 0",
    'button.hidden = !visible',
    'hint.hidden = !visible',
    "bootstrapAvailable = false",
):
    require(needle in web_http, f"first-Admin firmware visibility contract missing: {needle}")
require('JSON.stringify({ action: "bootstrap" })' not in web_http,
        "firmware-owned UI still probes bootstrap by issuing a fake POST")

for needle in (
    'api("/api/v1/access/status")',
    "refreshAccessStatus",
    "renderAccessStatus",
    'id="accessBootstrap" type="button" hidden aria-hidden="true"',
    'id="accessBootstrapHint" hidden aria-hidden="true"',
    "button.hidden || button.disabled",
):
    require(needle in web_app, f"base Web first-Admin visibility contract missing: {needle}")

for needle in (
    '"/api/v1/access/status"',
    ".method = HTTP_GET",
    "AccessHttp::status_get",
    "AccessHttp::handle_status",
    "bootstrapAllowed",
    "userCount",
    "bootstrap_allowed_ &&",
    "access_->user_count() == 0U",
):
    require(needle in access_http, f"read-only access status backend missing: {needle}")
require("if (!bootstrap_allowed_)" in access_http and "bootstrap_unavailable" in access_http,
        "bootstrap POST backend no longer independently rejects provisioned controllers")

# Password/PIN reveal controls are release invariants, not optional polish.
for needle in (
    "ensureSecretToggles",
    "operatorPin",
    "wifiPassword",
    "cloudPassword",
    "cloudCredential",
    "factoryCredential",
    "hg-secret-toggle",
):
    require(needle in web_http, f"secret show/hide contract missing: {needle}")

# Navigation must always retain exactly one active item and collapse on phone.
for needle in (
    "enforceSingleActiveNav",
    "lastSidebarLink",
    "installNavMutationGuard",
    "MutationObserver",
    "navRepairQueued",
    'attributeFilter: ["class"]',
    "classList.toggle(\"active\"",
    "mobile-menu-toggle",
    "mobile-menu-open",
    ".sidebar nav{display:none!important",
    ".sidebar.mobile-menu-open nav{display:grid!important",
    "object-fit:contain!important",
):
    require(needle in web_http, f"navigation/mobile field contract missing: {needle}")

# Backend must remain independently protected even if the browser is bypassed.
for needle in (
    '"/api/v1/system/factory-reset"',
    'confirm != "ERASE_ALL"',
    'authorize(actor, credential, "system.factory_reset")',
    "FactoryResetManager{}.erase_mutable_state()",
    "access_control_->clear_users()",
    "esp_restart()",
):
    require(needle in system_http, f"Factory Reset backend safety missing: {needle}")

if errors:
    print("Field Web acceptance FAIL")
    for error in errors:
        print(f" - {error}")
    sys.exit(1)

print("Field Web acceptance PASS")
print(" - Factory Reset: firmware-visible + Admin credentials + double confirm")
print(" - first Admin: read-only status API; button/hint fail-closed unless factory-fresh")
print(" - fake bootstrap POST capability probes are forbidden")
print(" - password/PIN show-hide controls: firmware-owned")
print(" - navigation: exactly-one-active + immediate mutation repair + collapsed mobile menu")
print(" - Bruce: contain, not cover")
print(" - backend: ERASE_ALL + access authorization + erase + reboot")
