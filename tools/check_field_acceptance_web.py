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
idf_cmake = (MAIN / "CMakeLists.txt").read_text(encoding="utf-8")
web_app = (ROOT / "web" / "app.js").read_text(encoding="utf-8")
access_session = (ROOT / "web" / "access-session.js").read_text(encoding="utf-8")
first_admin_hint = (ROOT / "web" / "first-admin-hint.js").read_text(encoding="utf-8")
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

# The primary firmware-owned reset UI must distinguish an explicit HTTP reject
# from transport loss while the controller is erasing Wi-Fi/rebooting. A socket
# loss is indeterminate, not proof that the destructive command failed.
for needle in (
    "let responseReceived = false",
    "responseReceived = true",
    "if (!responseReceived)",
    "Зв’язок обірвався під час Factory Reset",
    "Factory Reset відхилено:",
):
    require(needle in web_http, f"Factory Reset transport-loss UX missing: {needle}")
require(
    web_http.find("if (!responseReceived)") < web_http.find("button.disabled = false;", web_http.find("if (!responseReceived)")),
    "transport-loss path can re-enable destructive reset button before controller state is known",
)
require(
    'if (credential) credential.value = "";' in web_http,
    "Factory Reset does not clear Admin PIN after success/loss/rejection",
)

# The first-Admin action is allowed only on a truly factory-fresh controller.
# A transient request failure immediately after boot must remain fail-closed but
# MUST be retried; otherwise one boot race permanently hides the bootstrap UI.
for needle in (
    "syncBootstrapAvailability",
    "applyBootstrapVisibility",
    "bootstrapProbeInFlight",
    "bootstrapAvailable",
    "/api/v1/access/status",
    'method: "GET"',
    "body.bootstrapAllowed === true",
    "Number(body.userCount || 0) === 0",
    'button.hidden = !visible',
    'hint.hidden = !visible',
    "bootstrapAvailable = null",
    "setInterval(enforceAcceptanceUi, 500)",
):
    require(needle in web_http, f"first-Admin firmware visibility contract missing: {needle}")
require("bootstrapAvailable = false" not in web_http,
        "a transient access/status failure can permanently suppress first-Admin bootstrap")
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
    "setInterval(refreshFirstAdminHint, 1500)",
    "/api/v1/access/status?ts=",
    "if (!response.ok || body?.ok === false)",
    "showBanner()",
    "Створити першого Admin",
):
    require(needle in first_admin_hint, f"factory-fresh first-Admin retry/banner missing: {needle}")

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

# The approved Bruce bytes are PNG despite the historical /bruce.jpg URL.
# Serve them directly from the route; no global linker wrapping of HTTP calls.
for needle in (
    "send_binary_chunked",
    'return send_binary_chunked(request, "image/png", bruce_jpg_start, bruce_jpg_end);',
    "httpd_resp_send_chunk",
):
    require(needle in web_http, f"Bruce direct-serving contract missing: {needle}")
require("--wrap=httpd_resp" not in idf_cmake,
        "Bruce serving still depends on global HTTP linker wrappers")
require("hg_web_png_compat.cpp" not in idf_cmake,
        "obsolete Bruce compatibility wrapper is still compiled")
for needle in (
    "refreshBruceSource",
    "/bruce.jpg?rev=",
    'image.addEventListener("error"',
):
    require(needle in first_admin_hint, f"Bruce browser retry/cache-bust missing: {needle}")

# Firmware suffix also repairs hash navigation synchronously; the source
# access/session layer performs the same guarantee around duplicate href links.
require(
    'window.addEventListener("hashchange", () => {' in web_http and
    "applyEmbeddedView();\n    enforceSingleActiveNav();\n    queueMicrotask(enforceSingleActiveNav);" in web_http,
    "firmware hashchange repair is not synchronous",
)
require("function enforceSingleSidebarActive()" in access_session,
        "base access-session navigation repair is missing")
require(
    "lastSidebarLink = link;\n    // Repair immediately" in access_session and
    "enforceSingleSidebarActive();\n    queueMicrotask(enforceSingleSidebarActive);" in access_session,
    "sidebar click repair is not synchronous before microtask fallback",
)
require(
    'window.addEventListener("hashchange", () => {' in access_session and
    "// app.js may mark every link sharing this hash active" in access_session and
    "enforceSingleSidebarActive();\n    queueMicrotask(enforceSingleSidebarActive);" in access_session,
    "hashchange repair is not synchronous before microtask fallback",
)
require("new MutationObserver" in access_session and 'attributeFilter: ["class"]' in access_session,
        "base access-session lost mutation fallback for active navigation")

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
print(" - reset transport loss is indeterminate/offline; explicit HTTP rejection remains retryable")
print(" - first Admin: read-only status API + retry across boot-time HTTP races")
print(" - fake bootstrap POST capability probes are forbidden")
print(" - password/PIN show-hide controls: firmware-owned")
print(" - navigation: synchronous exact-one-active repair + mutation fallback + collapsed mobile menu")
print(" - Bruce: direct PNG chunk serving + cache-busted retry + contain, not cover")
print(" - backend: ERASE_ALL + access authorization + erase + reboot")
