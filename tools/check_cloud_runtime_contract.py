#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "esp-idf" / "main"

def read(name: str) -> str:
    return (MAIN / name).read_text(encoding="utf-8")

app = read("app_main.cpp")
link = read("hg_cloud_link.cpp")
http = read("hg_cloud_http.cpp")
nvs = read("hg_cloud_nvs.cpp")
trust_nvs = read("hg_cloud_trust_nvs.cpp")
cmake = read("CMakeLists.txt")
contract = (ROOT / "docs" / "CLOUD-BACKEND-CONTRACT.md").read_text(encoding="utf-8")
errors = []

def require(ok: bool, msg: str):
    if not ok: errors.append(msg)

require('"hg_cloud_nvs.cpp"' in cmake, "cloud NVS source not compiled")
require("g_cloud_store.load" in app and "restore_cloud_config" in app, "cloud config not restored at boot")
require("g_cloud_link.start" in app, "persisted cloud config does not auto-start MQTT")
require("set_command_runtime(&g_system_model, &g_system_bus, &g_access_control, &g_cloud_time)" in app, "live command runtime + trusted time not wired")
require('"/api/v1/cloud/status"' in http, "cloud status endpoint missing")
require('"/api/v1/cloud/config"' in http, "cloud config endpoint missing")
require("request_auth::authenticated_actor(request, *access_control_, actor)" in http,
        "cloud config endpoint does not authenticate Bearer actor")
require('access_control_->authorize_session(actor, "cloud.configure")' in http,
        "cloud config endpoint does not authorize Bearer session role")
require('access_control_->authorize(actor, credential, "cloud.configure")' not in http,
        "cloud config endpoint still re-checks acting PIN after Bearer login")
require("store_->save(config)" in http, "cloud config is not persisted")
config_start = http.find("esp_err_t CloudHttp::handle_config")
config_body = http[config_start:] if config_start >= 0 else ""
require("CloudTrustStore" not in config_body and "trust_store_" not in config_body and "hg_cloud_trust_nvs" not in config_body,
        "ordinary /cloud/config must not provision, rotate, or clear Cloud Command Trust")
require('"/api/v1/cloud/trust"' in http, "separate Cloud Command Trust endpoint missing")
trust_start = http.find("esp_err_t CloudHttp::handle_trust")
trust_body = http[trust_start:] if trust_start >= 0 else ""
require("request_auth::authenticated_admin(request, *access_control_)" in trust_body,
        "Cloud Command Trust provisioning is not admin-only")
require("trust_store_->save(trust)" in trust_body,
        "Cloud Command Trust endpoint does not persist trust material")
require("trust.version <= current.version" in trust_nvs and "ESP_ERR_INVALID_STATE" in trust_nvs,
        "Cloud Command Trust store does not reject version rollback")
require("cloud_->stop()" in http and "cloud_->start(" in http, "cloud config change does not restart MQTT")
require("responses" in link and "response_topic_" in link, "MQTT response topic missing")
require("handle_command(event->data" in link, "MQTT command payload is not routed")
require("trusted_time_->ready()" in link and "expires_at_ms" in link and "issued_at_ms" in link,
        "MQTT command freshness is not gated by trusted time")
challenge_branch = link.find('if (command == "security.disarm_challenge")')
replay_check = link.find("if (command_counter <= stored_counter)")
request_replay_check = link.find("if (request_id == last_request_id)")
challenge_persist = link.find("persist_command_replay_state(command_counter, request_id)", challenge_branch)
challenge_issue = link.find("issue_disarm_challenge()", challenge_branch)
require(challenge_branch >= 0, "signed disarm challenge issuance path missing")
require(replay_check >= 0 and request_replay_check >= 0 and replay_check < challenge_branch and request_replay_check < challenge_branch,
        "disarm challenge issuance bypasses persistent replay checks")
require(challenge_persist >= 0 and challenge_issue >= 0 and challenge_persist < challenge_issue,
        "disarm challenge replay state must persist before token issuance")
require("access_control_->authorize_session(actor, command)" in link,
        "MQTT commands do not authorize the signed actor through AccessControl session roles")
require('parse_json_string(body, "credential"' not in link and "authorize(actor, credential" not in link,
        "MQTT command path must not transport or re-check a user PIN credential")
require("model_->set_partition_arm" in link and "bus_->dispatch_all" in link, "MQTT security command does not reach live model")
require("deferred to safe command router" not in link, "old deferred MQTT command placeholder remains")
require("nvs_set_str" in nvs and "nvs_get_str" in nvs and "nvs_commit" in nvs, "cloud credentials are not persisted in NVS")

canonical_fields = [
    '"version"', '"deviceId"', '"requestId"', '"actor"', '"command"',
    '"counter"', '"issuedAtMs"', '"expiresAtMs"', '"challenge"', '"signature"',
]
for field in canonical_fields:
    require(field in contract, f"canonical MQTT contract missing {field}")
require("Factory identity" in contract and "MQTT credentials" in contract and "Cloud Command Trust identity" in contract,
        "MQTT trust identities are not explicitly separated")
require("must not be claimed as implemented until their CI gates are green" in contract,
        "contract must distinguish required MQTT security from implemented firmware")

if errors:
    print("Cloud runtime contract FAIL")
    for error in errors:
        print(" -", error)
    sys.exit(1)

print("Cloud runtime contract PASS")
print(" - persistent MQTT config + boot restore")
print(" - Bearer-session cloud configuration endpoint")
print(" - ordinary cloud config isolated from Cloud Command Trust")
print(" - admin-only monotonic Cloud Command Trust provisioning")
print(" - signed-actor AccessControl session authorization; no MQTT PIN credential")
print(" - MQTT responses topic")
print(" - replay-protected one-time disarm challenge issuance")
print(" - canonical signed-command contract + trust separation")
