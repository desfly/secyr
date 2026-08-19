from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
ESP = ROOT / "firmware" / "esp-idf"
MAIN = ESP / "main"
CORE = ROOT / "firmware" / "src"
WEB = ROOT / "web"
COMPONENTS = ESP / "components"

errors = []
warnings = []

rules = [
    (re.compile(r"\bESP_RETURN_ON_ERROR\b"), '#include "esp_check.h"', "ESP_RETURN_ON_ERROR requires esp_check.h"),
    (re.compile(r"\bpdMS_TO_TICKS\b"), '#include "freertos/FreeRTOS.h"', "pdMS_TO_TICKS requires FreeRTOS.h"),
    (re.compile(r"\bxTaskCreate\b|\bvTaskDelay\b"), '#include "freertos/task.h"', "FreeRTOS task API requires task.h"),
    (re.compile(r"\besp_rom_delay_us\b"), '#include "esp_rom_sys.h"', "esp_rom_delay_us requires esp_rom_sys.h"),
    (re.compile(r"\bstd::uint(?:8|16|32|64)_t\b"), "#include <cstdint>", "fixed-width integer type requires cstdint"),
    (re.compile(r"\bstd::size_t\b"), "#include <cstddef>", "std::size_t requires cstddef"),
]

for source in sorted(MAIN.glob("*.cpp")) + sorted(MAIN.glob("*.hpp")):
    text = source.read_text(encoding="utf-8")
    for pattern, include, message in rules:
        if pattern.search(text) and include not in text:
            errors.append(f"{source.name}: {message}")
    if "ESP_ERROR_CHECK(" in text and source.suffix == ".hpp":
        warnings.append(f"{source.name}: ESP_ERROR_CHECK found in header")
    if "new " in text or "malloc(" in text:
        warnings.append(f"{source.name}: dynamic allocation should be reviewed")
    if re.search(r"\bGPIO_NUM_(?:19|20|35|36|37|43|44|45|46|48)\b", text):
        warnings.append(f"{source.name}: reserved GPIO referenced")

access_http = (MAIN / "hg_access_http.cpp").read_text(encoding="utf-8")
large_access_stack_copy = re.compile(r"\b(?:const\s+)?(?:auto|(?:homeguard::)?AccessControl)\s+\w+\s*=\s*\*access_\s*;")
if large_access_stack_copy.search(access_http):
    errors.append("hg_access_http.cpp: AccessControl rollback snapshot must not be copied into the httpd task stack")
if "bool escaped = false" not in access_http or "scrub(credential);" not in access_http:
    errors.append("hg_access_http.cpp: login auth must use escaped JSON parsing and scrub the login credential")
if 'allowed("network.configure")' not in access_http or 'allowed("system.network.configure")' in access_http:
    errors.append("hg_access_http.cpp: networkConfigure capability must match network.configure authorization action")
if 'access_->authorize_session(actor, "access.manage")' not in access_http:
    errors.append("hg_access_http.cpp: user management must authorize the Bearer session role")
if 'access_->authorize(actor, credential, "access.manage"' in access_http:
    errors.append("hg_access_http.cpp: acting Admin PIN must not be re-checked after Bearer login")

access_store = (CORE / "access_store.cpp").read_text(encoding="utf-8")
full_access_local = re.compile(r"\bAccessControl\s+[A-Za-z_]\w*\s*(?:;|\{)")
if full_access_local.search(access_store):
    errors.append("access_store.cpp: full AccessControl temporary must not be placed on the NVS decode task stack")

http_util = (MAIN / "hg_http_util.hpp").read_text(encoding="utf-8")
if "bool escaped = false" not in http_util or "std::fill(secret.begin(), secret.end(), '\\0')" not in http_util:
    errors.append("hg_http_util.hpp: shared JSON parser/scrubber security contract is incomplete")

reset_sequence = (MAIN / "hg_reset_sequence.cpp").read_text(encoding="utf-8")
if "perform_early_boot_factory_reset" not in reset_sequence or "FactoryResetManager{}.erase_mutable_state()" not in reset_sequence:
    errors.append("hg_reset_sequence.cpp: staged reset is not consumed in the early-boot destructive path")
if not re.search(r"if\s*\(!report\.ok\(\)\)\s*\{.*?esp_restart\(\);.*?return\s*;", reset_sequence, re.S):
    errors.append("hg_reset_sequence.cpp: failed destructive reset path can return without esp_restart")
if reset_sequence.find("set_pending_reset(false)") < reset_sequence.find("FactoryResetManager{}.erase_mutable_state()"):
    errors.append("hg_reset_sequence.cpp: pending reset marker must survive until destructive erase succeeds")
if "stage_factory_reset_request()" not in reset_sequence or "esp_restart();" not in reset_sequence:
    errors.append("hg_reset_sequence.cpp: triple-hold gesture must stage reset and reboot into early-boot recovery")

system_http = (MAIN / "hg_system_http.cpp").read_text(encoding="utf-8")
for required in ("stage_factory_reset_request()", "schedule_factory_reboot()", '"rebooting\\\":true'):
    if required not in system_http:
        errors.append(f"hg_system_http.cpp: staged Web factory reset missing required step: {required}")
if "FactoryResetManager{}.erase_mutable_state()" in system_http:
    errors.append("hg_system_http.cpp: live HTTP factory reset must not erase mutable state directly")
if '#include "hg_http_util.hpp"' not in system_http or "http_util::parse_json_string" not in system_http or \
        "http_util::scrub(body);" not in system_http:
    errors.append("hg_system_http.cpp: System auth must use shared escaped JSON parsing and scrub request bodies")
if "access_control_->authorize_session(actor,command)" not in system_http or \
        'authorize_session(actor,"system.factory_reset")' not in system_http:
    errors.append("hg_system_http.cpp: protected commands must use Bearer-session role authorization")
registered_system_routes = system_http.split("esp_err_t SystemHttp::send_json", 1)[0]
if '"/ws/system"' in registered_system_routes:
    errors.append("hg_system_http.cpp: unauthenticated /ws/system event stream must remain disabled")

factory_reset_js = (WEB / "factory-reset.js").read_text(encoding="utf-8")
if "body.rebooting === true" not in factory_reset_js:
    errors.append("factory-reset.js: staged destructive reset reboot response is not handled")
if "credential: credential.value" in factory_reset_js:
    errors.append("factory-reset.js: acting Admin PIN must not be transmitted after login")

network_http = (MAIN / "hg_network_http.cpp").read_text(encoding="utf-8")
if "while (offset < body.size())" not in network_http or "body.data() + offset" not in network_http:
    errors.append("hg_network_http.cpp: Wi-Fi connect handler must read the complete declared HTTP body")
if "wifi_persist_failed" not in network_http or "have_previous_sta" not in network_http:
    errors.append("hg_network_http.cpp: failed Wi-Fi NVS commit must restore the previous live STA config")
if not re.search(r"if\s*\(!save_credentials\(ssid, password\)\)\s*\{.*?esp_wifi_set_config\(WIFI_IF_STA,\s*&previous_sta\)", network_http, re.S):
    errors.append("hg_network_http.cpp: Wi-Fi persistence rollback guard is missing")
if 'access_->authorize_session(actor, "network.configure")' not in network_http:
    errors.append("hg_network_http.cpp: Wi-Fi changes must use Bearer-session role authorization")

cloud_http = (MAIN / "hg_cloud_http.cpp").read_text(encoding="utf-8")
if "read_request_body(request, 1024U, body)" not in cloud_http or "while (offset < body.size())" not in cloud_http:
    errors.append("hg_cloud_http.cpp: Cloud config handler must read the complete declared HTTP body")
if "rolledBack" not in cloud_http:
    errors.append("hg_cloud_http.cpp: MQTT start failure must expose successful rollback")
if "had_previous ? store_->save(previous) : store_->clear()" not in cloud_http:
    errors.append("hg_cloud_http.cpp: failed MQTT runtime start must restore the previous persisted config")
if "restore_runtime_error = cloud_->start(" not in cloud_http:
    errors.append("hg_cloud_http.cpp: failed MQTT runtime start must restore the previous live config")
if 'access_control_->authorize_session(actor, "cloud.configure")' not in cloud_http:
    errors.append("hg_cloud_http.cpp: Cloud changes must use Bearer-session role authorization")

output_http = (MAIN / "hg_output_http.cpp").read_text(encoding="utf-8")
if '#include "hg_http_util.hpp"' not in output_http or \
        "http_util::read_body(request, 384U, body)" not in output_http or \
        "http_util::parse_json_string(body, \"actor\", actor)" not in output_http or \
        "http_util::scrub(body);" not in output_http:
    errors.append("hg_output_http.cpp: output command must use the shared HTTP parser/scrubber")
if "value_offset" not in output_http or "http_util::value_offset(body, key)" not in output_http:
    errors.append("hg_output_http.cpp: output JSON scalar parser must delegate whitespace handling to shared helper")
if "access_control_->authorize_session(actor, command)" not in output_http:
    errors.append("hg_output_http.cpp: output command must use Bearer-session role authorization")
if "std::string credential" in output_http:
    errors.append("hg_output_http.cpp: acting credential must not exist in v2 output runtime")

telemetry_http = (MAIN / "hg_telemetry_session_http.cpp").read_text(encoding="utf-8")
if '#include "hg_http_util.hpp"' not in telemetry_http or \
        "http_util::read_body(request, 256U, body)" not in telemetry_http or \
        "http_util::parse_json_string(body, \"actor\", actor)" not in telemetry_http:
    errors.append("hg_telemetry_session_http.cpp: telemetry request parsing must use the shared escaped-JSON helper")
if "http_util::scrub(body);" not in telemetry_http or "http_util::scrub(token);" not in telemetry_http:
    errors.append("hg_telemetry_session_http.cpp: telemetry request body and issued token must be scrubbed after use")
if '#include "hg_request_auth.hpp"' not in telemetry_http or \
        "request_auth::authenticated_actor(request, *access_, actor)" not in telemetry_http:
    errors.append("hg_telemetry_session_http.cpp: telemetry ticket must derive from an authenticated Bearer session")
if "std::string credential" in telemetry_http:
    errors.append("hg_telemetry_session_http.cpp: telemetry v2 must not accept the acting PIN")

service_http = (MAIN / "hg_service_http.cpp").read_text(encoding="utf-8")
if '#include "hg_http_util.hpp"' not in service_http or "http_util::parse_json_string" not in service_http or \
        "http_util::scrub(body);" not in service_http:
    errors.append("hg_service_http.cpp: service auth must use shared escaped JSON parsing and scrub request body")
if 'self->access_control_->authorize_session(actor, "system.service.invalidate")' not in service_http:
    errors.append("hg_service_http.cpp: service invalidation must use Bearer-session role authorization")
if "std::string credential" in service_http:
    errors.append("hg_service_http.cpp: acting credential must not exist in v2 service runtime")

infrastructure_http = (MAIN / "hg_infrastructure_http.cpp").read_text(encoding="utf-8")
registered_part = infrastructure_http.split("esp_err_t InfrastructureHttp::rgb_test_post", 1)[0]
if "/api/v1/diagnostics/rgb-test" in registered_part:
    errors.append("hg_infrastructure_http.cpp: unauthenticated remote RGB diagnostic route must remain disabled")
if "remote_rgb_test_disabled" not in infrastructure_http:
    errors.append("hg_infrastructure_http.cpp: disabled RGB handler must remain fail-closed")

app_main = (MAIN / "app_main.cpp").read_text(encoding="utf-8")
for required in ("rollback_http", "g_system_http.detach_transport()", "httpd_stop(g_http_server)", "g_http_server = nullptr"):
    if required not in app_main:
        errors.append(f"app_main.cpp: partial HTTP registration rollback missing required step: {required}")
if "return g_build_http.register_handlers(g_http_server);" in app_main:
    errors.append("app_main.cpp: final build route must also participate in HTTP rollback")

stale_commissioning_http = MAIN / "hg_commissioning_http.hpp"
if stale_commissioning_http.exists():
    errors.append("hg_commissioning_http.hpp: stale unimplemented commissioning HTTP API must remain removed")

ws_component = COMPONENTS / "websocket_telemetry"
ws_cpp = (ws_component / "websocket_telemetry.cpp").read_text(encoding="utf-8")
ws_hpp = (ws_component / "include/websocket_telemetry.hpp").read_text(encoding="utf-8")
if "kSessionTokenLifetimeUs" not in ws_cpp or "esp_timer_get_time()" not in ws_cpp:
    errors.append("websocket_telemetry: session handshake tickets must have a bounded lifetime")
if not re.search(r"if\s*\(session\.authorized\(authorization\)\)\s*\{.*?session\.clear\(\);.*?return true;", ws_cpp, re.S):
    errors.append("websocket_telemetry: session handshake ticket must be single-use")
if "session_token_issued_us_" not in ws_hpp:
    errors.append("websocket_telemetry.hpp: session ticket issue timestamps are missing")

for warning in warnings:
    print(f"WARNING: {warning}")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("ESP-IDF source audit PASS")
