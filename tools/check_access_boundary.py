from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "esp-idf" / "main"
WEB = ROOT / "web"

errors = []

def require(path: Path, snippets: list[str]) -> None:
    text = path.read_text(encoding="utf-8")
    for snippet in snippets:
        if snippet not in text:
            errors.append(f"{path.relative_to(ROOT)} missing security contract: {snippet}")

# Inspect semantic tokens rather than the source spelling of escaped JSON quotes.
# C++ JSON literals use \\" in source, so matching quoted JSON strings directly
# made this gate fail when formatting changed despite identical runtime behavior.
require(MAIN / "hg_access_http.cpp", [
    '"/api/v1/access/state"', "access_runtime::setup_required", "setup_required", "login_required",
    "http_session::issue(user->id.data(), user->role)", "sessionToken",
    '"/api/v1/access/logout"', "http_session::revoke(authorization)",
])

require(MAIN / "hg_access_runtime.hpp", ["g_bootstrap_allowed", "setup_required", "lock_bootstrap"])
access_http = (MAIN / "hg_access_http.cpp").read_text(encoding="utf-8")
if "bootstrap_allowed_ = false" not in access_http:
    errors.append("hg_access_http.cpp must explicitly close bootstrap only in first-Admin flow")
if "bootstrap_allowed_ = true" not in access_http:
    errors.append("hg_access_http.cpp must restore bootstrap when first-Admin creation/persistence fails")

app_main = (MAIN / "app_main.cpp").read_text(encoding="utf-8")
if "nvs_flash_erase()" in app_main:
    errors.append("app_main.cpp must not implicitly erase NVS during ordinary boot")
require(MAIN / "app_main.cpp", [
    "access_runtime::set_bootstrap_allowed(false)",
    "access_runtime::set_bootstrap_allowed(true)",
    "enter_nvs_recovery_mode(nvs_error)",
])

require(MAIN / "hg_nvs_recovery.cpp", [
    "kRequiredHolds = 3U",
    "set_white(board::kOnboardRgb)",
    "nvs_flash_erase()",
    "set_red(board::kOnboardRgb)",
])
recovery = (MAIN / "hg_nvs_recovery.cpp").read_text(encoding="utf-8")
if "esp_netif_init" in recovery or "httpd_" in recovery or "esp_wifi" in recovery:
    errors.append("NVS recovery must remain isolated from network/HTTP")

for filename in (
    "hg_system_http.cpp", "hg_cloud_http.cpp", "hg_lan_http.cpp",
    "hg_infrastructure_http.cpp", "hg_build_http.cpp", "hg_service_http.cpp",
):
    require(MAIN / filename, ["hg_request_auth.hpp", "request_auth::"])

require(MAIN / "hg_network_http.cpp", [
    "hg_access_runtime.hpp", "hg_request_auth.hpp",
    "access_runtime::setup_required", "request_auth::",
])

require(MAIN / "hg_request_auth.hpp", [
    "hg_http_session.hpp", "http_session::authorized(authorization, access)", "WWW-Authenticate",
])
request_auth = (MAIN / "hg_request_auth.hpp").read_text(encoding="utf-8")
if "read_legacy_authorization" in request_auth or 'prefix{"HomeGuard "}' in request_auth:
    errors.append("hg_request_auth.hpp must not permit legacy actor:PIN authorization on protected APIs")
if "access.authenticate(actor, credential)" in request_auth:
    errors.append("hg_request_auth.hpp protected reads must require login-issued Bearer sessions")

require(MAIN / "hg_http_session.hpp", [
    "kLifetimeUs", "BearerTokenVerifier", "g_actors", "g_roles",
    "issue(std::string_view actor, homeguard::AccessRole role)",
    "authorized(std::string_view authorization, homeguard::AccessControl& access)",
    "access.find_user(g_actors[i].data())", "user->role != g_roles[i]",
    "revoke(", "revoke_all",
])

# Release-critical mutating APIs must still pass through command-level RBAC.
require(MAIN / "hg_system_http.cpp", [
    'authorize(actor,credential,"system.factory_reset")',
    "access_control_->authorize(actor,credential,command)",
])
require(MAIN / "hg_network_http.cpp", [
    'access_->authorize(actor, credential, "network.configure")',
    "had_persisted_credentials = load_credentials(previous_ssid, previous_password)",
    "persist_rollback_ok = had_persisted_credentials",
    "? save_credentials(previous_ssid, previous_password)",
    ": clear_credentials()",
    "wifi_connect_failed_rollback_failed",
    '"rolledBack\\\":true}',
])
network_http = (MAIN / "hg_network_http.cpp").read_text(encoding="utf-8")
connect_call = network_http.find("const auto connect_error = esp_wifi_connect()")
success_reply = network_http.find('"ok\\\":true,\\\"state\\\":\\\"connecting"')
if connect_call < 0 or success_reply < 0 or success_reply < connect_call:
    errors.append("Wi-Fi connect API must not report success before esp_wifi_connect() has been accepted")
require(MAIN / "hg_network_http.hpp", ["bool clear_credentials() const;"])
require(MAIN / "hg_output_http.cpp", [
    "access_control_->authorize(actor, credential, command)",
])

system_http = (MAIN / "hg_system_http.cpp").read_text(encoding="utf-8")
if "stage_factory_reset_request()" not in system_http:
    errors.append("hg_system_http.cpp must stage factory reset for early boot")
if "FactoryResetManager{}.erase_mutable_state()" in system_http:
    errors.append("hg_system_http.cpp must not erase mutable state in live HTTP runtime")

reset = (MAIN / "hg_reset_sequence.cpp").read_text(encoding="utf-8")
for snippet in (
    "set_red(board::kOnboardRgb)", "stage_factory_reset_request()",
    "set_white(board::kOnboardRgb)", "kRequiredHolds = 3U",
    "kHoldTicks = pdMS_TO_TICKS(1500)", "kSuccessWhiteTicks = pdMS_TO_TICKS(5000)",
    "FactoryResetManager{}.erase_mutable_state()", "set_pending_reset(false)",
    "RED means release now", "WHITE RGB confirmation for 5 seconds",
):
    if snippet not in reset:
        errors.append(f"hg_reset_sequence.cpp missing reset contract: {snippet}")
if reset.find("set_pending_reset(false)") < reset.find("FactoryResetManager{}.erase_mutable_state()"):
    errors.append("hg_reset_sequence.cpp must keep factory-reset pending until erase succeeds")

# RED belongs to the live button-hold phase; WHITE belongs to the later early-boot
# success phase after mutable state is erased. Do not compare their first lexical
# positions in the file: helper functions may be ordered independently.
service_start = reset.find("void service_button_reset_task")
service_end = reset.find("\n}\n\n}  // namespace", service_start)
early_start = reset.find("void perform_early_boot_factory_reset")
early_end = reset.find("\n}\n\nvoid stage_factory_reset_and_reboot", early_start)
if service_start < 0 or service_end < 0 or "RgbDiagnostic::set_red(board::kOnboardRgb)" not in reset[service_start:service_end]:
    errors.append("hg_reset_sequence.cpp RED confirmation must remain in service-button hold runtime")
if early_start < 0 or early_end < 0 or "RgbDiagnostic::set_white(board::kOnboardRgb)" not in reset[early_start:early_end]:
    errors.append("hg_reset_sequence.cpp WHITE confirmation must remain in successful early-boot reset runtime")

require(WEB / "access-session.js", [
    "hg-auth-locked", "/api/v1/access/state", "/api/v1/access/login",
    "sessionToken", "Bearer ${session.token}", "syncActorFields",
    "hgSetupWifiScan", "hgSetupWifiConnect", "/api/v1/network/connect",
])
web_session = (WEB / "access-session.js").read_text(encoding="utf-8")
if "session.credential" in web_session or "syncLegacyCredentials" in web_session:
    errors.append("web access session must not retain or auto-fill the login PIN")
if "session={actor:String(body.actor||actor),credential" in web_session:
    errors.append("web access session object must not store the login credential")
if 'credential="";session={' not in web_session:
    errors.append("web login credential must be cleared before establishing the long-lived session object")
require(WEB / "app.css", [".shell{visibility:hidden", "body:has(#hgAuthGate[hidden]) .shell{visibility:visible}"])

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("Access boundary security audit PASS")
