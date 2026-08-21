from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "esp-idf" / "main"
WEB = ROOT / "web"
CORE = ROOT / "firmware" / "src"
INCLUDE = ROOT / "firmware" / "include" / "homeguard"

errors = []

def require(path: Path, snippets: list[str]) -> None:
    text = path.read_text(encoding="utf-8")
    for snippet in snippets:
        if snippet not in text:
            errors.append(f"{path.relative_to(ROOT)} missing security contract: {snippet}")

# Login is the only place where an acting user's PIN is verified. Protected
# requests use an actor-bound Bearer session and command-level role policy.
require(INCLUDE / "access_control.hpp", [
    "authorize_session(",
    "Session model (v2)",
    "LEGACY v1",
])
require(CORE / "access_control.cpp", [
    "AuditDecision AccessControl::authorize_session(",
    "role_allows(user->role, command)",
    "append_audit(actor, command, AuditDecision::Allowed)",
    "LEGACY v1",
])

require(MAIN / "hg_access_http.cpp", [
    '"/api/v1/access/state"', "access_runtime::setup_required", "setup_required", "login_required",
    "http_session::issue(user->id.data(), user->role)", "sessionToken",
    '"/api/v1/access/logout"', "http_session::revoke(authorization)",
    "http_session::authorized_for_actor(authorization, *access_, actor)",
    'access_->authorize_session(actor, "access.manage")',
    "std::unique_ptr<AccessControl> previous_access",
    "*access_ = *previous_access",
    "const auto persist = store_->save(*access_)",
    "http_session::revoke_actor(id)",
])

require(MAIN / "hg_access_runtime.hpp", ["g_bootstrap_allowed", "setup_required", "lock_bootstrap"])
access_http = (MAIN / "hg_access_http.cpp").read_text(encoding="utf-8")
if "bootstrap_allowed_ = false" not in access_http:
    errors.append("hg_access_http.cpp must explicitly close bootstrap only in first-Admin flow")
if "bootstrap_allowed_ = true" not in access_http:
    errors.append("hg_access_http.cpp must restore bootstrap when first-Admin creation/persistence fails")
if access_http.find("http_session::revoke_actor(id)") < access_http.find("const auto persist = store_->save(*access_)"):
    errors.append("user mutation must invalidate the changed account only after persistence succeeds")
if access_http.count("http_session::revoke_all()") != 1:
    errors.append("revoke_all must remain limited to first-Admin bootstrap; ordinary user edits use revoke_actor")
if 'access_->authorize(actor, credential, "access.manage"' in access_http:
    errors.append("access management must not re-check acting Admin PIN after Bearer login")

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
    "hg_http_session.hpp", "http_session::authorized(authorization, access)",
    "authenticated_actor", "http_session::authorized_for_actor", "WWW-Authenticate",
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
    "authorized_for_actor", "authorized_impl", "session_actor != expected_actor",
    "access.find_user(session_actor)", "user->role != g_roles[i]",
    "revoke(", "revoke_actor", "revoke_all",
])

# Release-critical mutating APIs: actor-bound Bearer + session role RBAC.
require(MAIN / "hg_system_http.cpp", [
    "request_auth::authenticated_actor(request, *access_control_, actor)",
    'authorize_session(actor,"system.factory_reset")',
    "access_control_->authorize_session(actor,command)",
    'confirm != "ERASE_ALL"',
])
require(MAIN / "hg_network_http.cpp", [
    "request_auth::authenticated_actor(request, *access_, actor)",
    'access_->authorize_session(actor, "network.configure")',
    "save_candidate_credentials(context.ssid, context.password)",
    "httpd_req_async_handler_begin",
    "httpd_req_async_handler_complete",
    "handover_pending",
    "current_ap_matches(candidate_ssid)",
    "save_credentials(candidate_ssid, candidate_password)",
])
network_http = (MAIN / "hg_network_http.cpp").read_text(encoding="utf-8")
response_call = network_http.find("const auto response_error = send_json")
complete_call = network_http.find("const auto complete_error = request_guard.complete()", response_call if response_call >= 0 else 0)
handover_delay = network_http.find("vTaskDelay(kStaHandoverDelay)", complete_call if complete_call >= 0 else 0)
disconnect_call = network_http.find("esp_wifi_disconnect()", handover_delay if handover_delay >= 0 else 0)
set_config_call = network_http.find("set_sta_config(context.ssid, context.password)", disconnect_call if disconnect_call >= 0 else 0)
connect_call = network_http.find("const auto connect_error = esp_wifi_connect()", set_config_call if set_config_call >= 0 else 0)
if min(response_call, complete_call, handover_delay, disconnect_call, set_config_call, connect_call) < 0 or not (
    response_call < complete_call < handover_delay < disconnect_call < set_config_call < connect_call
):
    errors.append("Wi-Fi connect API must complete HTTP acknowledgement before mutating or dropping STA transport")

process_start = network_http.find("void NetworkHttp::process_connect")
process_end = network_http.find("bool NetworkHttp::start_candidate_timeout", process_start)
process_connect = network_http[process_start:process_end] if process_start >= 0 and process_end > process_start else ""
if "save_credentials(" in process_connect or "clear_credentials()" in process_connect:
    errors.append("Wi-Fi candidate flow must not mutate committed credentials before verified GOT_IP")

ip_start = network_http.find("void NetworkHttp::on_ip_event")
ip_end = network_http.find("bool NetworkHttp::set_sta_config", ip_start)
ip_handler = network_http[ip_start:ip_end] if ip_start >= 0 and ip_end > ip_start else ""
match_pos = ip_handler.find("current_ap_matches(candidate_ssid)")
commit_pos = ip_handler.find("save_credentials(candidate_ssid, candidate_password)")
if match_pos < 0 or commit_pos < 0 or match_pos > commit_pos:
    errors.append("Wi-Fi candidate must verify the associated SSID before committing credentials")

require(MAIN / "hg_network_http.hpp", ["bool clear_credentials() const;", "bool current_ap_matches(const std::string& ssid) const;"])
require(MAIN / "hg_output_http.cpp", [
    "hg_request_auth.hpp",
    "request_auth::authenticated_actor(request, *access_control_, actor)",
    "access_control_->authorize_session(actor, command)",
    "physical_->force_safe()",
    "model_->set_output_active(output_id, false, 0)",
    "physical_output_failure",
])
require(MAIN / "hg_cloud_http.cpp", [
    "request_auth::authenticated_actor(request, *access_control_, actor)",
    'access_control_->authorize_session(actor, "cloud.configure")',
])
require(MAIN / "hg_service_http.cpp", [
    "request_auth::authenticated_actor(request, *self->access_control_, actor)",
    'self->access_control_->authorize_session(actor, "system.service.invalidate")',
])

# Active protected endpoint code must not use the old per-request PIN API.
for filename, legacy_call in (
    ("hg_network_http.cpp", 'access_->authorize(actor, credential, "network.configure")'),
    ("hg_cloud_http.cpp", 'access_control_->authorize(actor, credential, "cloud.configure")'),
    ("hg_service_http.cpp", 'self->access_control_->authorize(actor, credential, "system.service.invalidate")'),
):
    source = (MAIN / filename).read_text(encoding="utf-8")
    if legacy_call in source:
        errors.append(f"{filename} still contains active legacy per-request PIN authorization")

system_http = (MAIN / "hg_system_http.cpp").read_text(encoding="utf-8")
if "stage_factory_reset_request()" not in system_http:
    errors.append("hg_system_http.cpp must stage factory reset for early boot")
if "FactoryResetManager{}.erase_mutable_state()" in system_http:
    errors.append("hg_system_http.cpp must not erase mutable state in live HTTP runtime")

# Physical RST/EN is a security boundary because it authorizes destructive reset
# without a network session. Keep the boot path staged and fail-closed.
reset = (MAIN / "hg_reset_sequence.cpp").read_text(encoding="utf-8")
for snippet in (
    "handle_physical_rst_factory_reset()",
    "RTC_NOINIT_ATTR",
    "esp_reset_reason()",
    "ESP_RST_POWERON",
    "hg::reset_press_detected(",
    "hg::advance_reset_sequence(",
    'kSequenceNamespace = "hg_rstseq"',
    "RgbDiagnostic::set_white(board::kOnboardRgb)",
    "stage_factory_reset_request()",
    "FactoryResetManager{}.erase_mutable_state()",
    "set_pending_reset(false)",
    "RgbDiagnostic::set_red(board::kOnboardRgb)",
    "vTaskDelay(kSuccessRedTicks)",
):
    if snippet not in reset:
        errors.append(f"hg_reset_sequence.cpp missing physical RST security contract: {snippet}")

if "board::kServiceButton" in reset or "gpio_get_level(" in reset or "service_button_reset_task" in reset:
    errors.append("hg_reset_sequence.cpp must not substitute GPIO21/service-button logic for physical RST/EN")

if reset.find("set_pending_reset(false)") < reset.find("FactoryResetManager{}.erase_mutable_state()"):
    errors.append("hg_reset_sequence.cpp must keep factory-reset pending until erase succeeds")

white = reset.find("RgbDiagnostic::set_white(board::kOnboardRgb)")
persist = reset.find("store_sequence_count(step.count)", white)
if white < 0 or persist < 0 or white > persist:
    errors.append("physical RST sequence must show WHITE acknowledgement before persisting a step")

third = reset.find('Physical RST accepted: WHITE acknowledgement, step 3/3')
stage = reset.find("stage_factory_reset_request()", third)
restart = reset.find("esp_restart();", stage)
if min(third, stage, restart) < 0 or not (third < stage < restart):
    errors.append("third physical RST must stage Factory Reset before reboot")

erase = reset.find("FactoryResetManager{}.erase_mutable_state()")
consume = reset.find("set_pending_reset(false)", erase)
red = reset.find("RgbDiagnostic::set_red(board::kOnboardRgb)", consume)
red_delay = reset.find("vTaskDelay(kSuccessRedTicks)", red)
if min(erase, consume, red, red_delay) < 0 or not (erase < consume < red < red_delay):
    errors.append("successful Factory Reset must erase, consume pending, then confirm RED for 5 seconds")

require(WEB / "access-session.js", [
    "hg-auth-locked", "/api/v1/access/state", "/api/v1/access/login",
    "sessionToken", "Bearer ${session.token}", "syncActorFields",
    "bearerMutationRoutes", "payload.actor = session.actor", "delete payload.credential",
    "hgSetupWifiScan", "hgSetupWifiConnect", "/api/v1/network/connect",
])
web_session = (WEB / "access-session.js").read_text(encoding="utf-8")
if "session.credential" in web_session or "syncLegacyCredentials" in web_session:
    errors.append("web access session must not retain or auto-fill the login PIN")
if "session={actor:String(body.actor||actor),credential" in web_session:
    errors.append("web access session object must not store the login credential")
if 'credential="";session={' not in web_session:
    errors.append("web login credential must be cleared before establishing the long-lived session object")
if "delete payload.credential" not in web_session:
    errors.append("protected Web mutations must strip legacy credential before transmission")

require(WEB / "factory-reset.js", [
    "HomeGuardAuth", 'role?.() === "admin"', "ERASE_ALL",
    "actor: window.HomeGuardAuth.actor()",
])
factory_reset_js = (WEB / "factory-reset.js").read_text(encoding="utf-8")
if "credential: credential.value" in factory_reset_js:
    errors.append("factory reset Web request must not transmit acting Admin PIN")

require(WEB / "app.css", [".shell{visibility:hidden", "body:has(#hgAuthGate[hidden]) .shell{visibility:visible}"])

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("Access boundary security audit PASS")
print(" - login is the only acting-user PIN verification step")
print(" - protected mutations require actor-bound Bearer + role RBAC")
print(" - Wi-Fi candidate commit is actor-bound, transactional, and verified by associated SSID")
print(" - destructive physical reset remains RST/EN-only, staged, and RGB-confirmed")
