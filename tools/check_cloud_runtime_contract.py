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
verifier = read("hg_cloud_command_verifier.cpp")
cmake = read("CMakeLists.txt")
contract = (ROOT / "docs" / "CLOUD-BACKEND-CONTRACT.md").read_text(encoding="utf-8")
errors = []

def require(ok: bool, msg: str):
    if not ok: errors.append(msg)

require("std::string owned_public_key{public_key_pem}" in verifier and
        "owned_public_key.c_str()" in verifier and
        "owned_public_key.size() + 1U" in verifier,
        "command verifier must own and NUL-terminate PEM string_view input before mbedTLS parsing")
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
require('parse_json_u64(body, "keyEpoch", key_epoch)' in link and
        'if (key_epoch != trust.version)' in link and
        'publish_response(false, "key_epoch_rejected")' in link,
        "MQTT command is not bound to the active Cloud Command Trust key epoch")
key_epoch_check = link.find("if (key_epoch != trust.version)")
key_epoch_canonical = link.find('"keyEpoch=" + std::to_string(key_epoch)')
replay_lock_for_epoch = link.find("xSemaphoreTake(replay_mutex, portMAX_DELAY)")
require(key_epoch_check >= 0 and key_epoch_canonical >= 0 and replay_lock_for_epoch >= 0 and
        key_epoch_check < key_epoch_canonical < replay_lock_for_epoch,
        "key epoch must be checked, signed, and rejected before replay admission")
# Trust must be checked again inside replay admission: a rotation may occur
# while the original signature verification is in progress.
admission_lock = link.find("xSemaphoreTake(replay_mutex, portMAX_DELAY)")
admission_reload = link.find("trust_store.load(admission_trust)", admission_lock)
admission_epoch = link.find("admission_trust.version != trust.version", admission_reload)
admission_key = link.find("admission_trust.public_key_pem != trust.public_key_pem", admission_reload)
admission_reject = link.find('publish_response(false, "key_epoch_changed")', admission_key)
admission_counter = link.find("load_command_counter(stored_counter)", admission_reject)
require(admission_lock >= 0 and admission_reload >= 0 and admission_epoch >= 0 and
        admission_key >= 0 and admission_reject >= 0 and admission_counter >= 0 and
        admission_lock < admission_reload < admission_epoch < admission_key <
        admission_reject < admission_counter,
        "MQTT trust must be revalidated under replay admission before replay state is read")

# Trust provisioning and command execution must share the same mutex. The
# replay mutex must not be released between durable admission and side effects.
require("bool CloudLink::begin_trust_rotation()" in link and
        "xSemaphoreTake(mutex, portMAX_DELAY)" in link and
        "void CloudLink::end_trust_rotation()" in link,
        "cloud trust rotation gate must use the command admission mutex")
require("cloud_->begin_trust_rotation()" in http and
        "cloud_->end_trust_rotation()" in http and
        http.find("cloud_->begin_trust_rotation()") < http.find("trust_store_->save(trust)") <
        http.find("cloud_->end_trust_rotation()"),
        "trust writer must hold the shared gate while persisting a new key")
challenge_start = link.find('if (command == "security.disarm_challenge")')
challenge_commit = link.find("persist_command_replay_state(command_counter, request_id)", challenge_start)
challenge_issue_locked = link.find("if (issue_disarm_challenge() != ESP_OK)", challenge_commit)
challenge_unlock = link.find("xSemaphoreGive(replay_mutex);", challenge_issue_locked)
require(challenge_commit >= 0 and challenge_issue_locked > challenge_commit and
        "xSemaphoreGive(replay_mutex);" not in link[challenge_commit:challenge_issue_locked] and
        challenge_unlock > challenge_issue_locked,
        "disarm challenge must remain locked from replay commit through issuance")
command_commit = link.find("persist_command_replay_state(command_counter, request_id)", challenge_issue_locked)
command_effect = link.find("model_->set_partition_arm(1, target, 0)", command_commit)
command_unlock = link.find("xSemaphoreGive(replay_mutex);", command_effect)
require(command_commit >= 0 and command_effect > command_commit and
        "xSemaphoreGive(replay_mutex);" not in link[command_commit:command_effect] and
        command_unlock > command_effect,
        "security command must remain locked from replay commit through side effect")

# The post-commit trust check must precede any arm-state side effect.
execution_reload = link.find("trust_store.load(execution_trust)")
execution_reject = link.find('publish_response(false, "key_epoch_changed")', execution_reload)
execution_disarm = link.find('if (command == "security.disarm") consume_disarm_challenge()', execution_reject)
execution_arm = link.find("model_->set_partition_arm(1, target, 0)", execution_disarm)
require(execution_reload >= 0 and execution_reject >= 0 and
        execution_disarm >= 0 and execution_arm >= 0 and
        execution_reload < execution_reject < execution_disarm < execution_arm,
        "MQTT command side effects must follow the post-commit trust recheck")

# Opening an existing replay namespace may fail for reasons other than first boot.
# Never interpret an NVS I/O/corruption error as an empty replay history.
for loader in ("load_command_counter", "load_last_request_id"):
    begin = link.find("bool " + loader + "(")
    end = link.find("\n}\n", begin)
    require(begin >= 0 and end > begin, f"{loader} implementation missing")
    if begin >= 0 and end > begin:
        body = link[begin:end]
        require("open_error == ESP_ERR_NVS_NOT_FOUND" in body and
                "if (open_error != ESP_OK) return false;" in body and
                "if (nvs_open(" not in body,
                f"{loader} must fail closed on NVS open errors other than missing namespace")

require("xSemaphoreCreateMutexStatic" in link and
        "xSemaphoreTake(replay_mutex, portMAX_DELAY)" in link and
        "xSemaphoreGive(replay_mutex)" in link,
        "MQTT replay admission must be serialized by a FreeRTOS mutex")
replay_lock = link.find("xSemaphoreTake(replay_mutex, portMAX_DELAY)")
replay_counter_load = link.find("load_command_counter(stored_counter)", replay_lock)
replay_request_load = link.find("load_last_request_id(last_request_id)", replay_lock)
replay_counter_check = link.find("if (command_counter <= stored_counter)", replay_lock)
replay_persist = link.find("persist_command_replay_state(command_counter, request_id)", replay_lock)
replay_unlock = link.find("xSemaphoreGive(replay_mutex)", replay_persist)
require(replay_lock >= 0 and replay_counter_load >= 0 and replay_request_load >= 0 and
        replay_counter_check >= 0 and replay_persist >= 0 and replay_unlock >= 0 and
        replay_lock < replay_counter_load <= replay_request_load < replay_counter_check < replay_persist < replay_unlock,
        "MQTT replay admission ordering must be lock -> read -> validate -> durable commit -> unlock")
challenge_branch = link.find('if (command == "security.disarm_challenge")')
replay_check = link.find("if (command_counter <= stored_counter)")
request_replay_check = link.find("if (request_id == last_request_id)")
challenge_persist = link.find("persist_command_replay_state(command_counter, request_id)", challenge_branch)
challenge_issue = link.find("issue_disarm_challenge()", challenge_branch)
require(challenge_branch >= 0, "signed disarm challenge issuance path missing")
require(replay_check >= 0 and request_replay_check >= 0 and replay_check < challenge_branch and request_replay_check < challenge_branch,
        "disarm challenge issuance bypasses persistent replay checks")
challenge_trust_reload = link.find("trust_store.load(challenge_trust)", challenge_persist)
challenge_trust_reject = link.find('publish_response(false, "key_epoch_changed")', challenge_trust_reload)
require(challenge_persist >= 0 and challenge_trust_reload >= 0 and
        challenge_trust_reject >= 0 and challenge_issue >= 0 and
        challenge_persist < challenge_trust_reload < challenge_trust_reject < challenge_issue,
        "disarm challenge must recheck trust after replay commit and before issuance")
require(challenge_persist >= 0 and challenge_issue >= 0 and challenge_persist < challenge_issue,
        "disarm challenge replay state must persist before token issuance")
require("kDisarmChallengeLength = 32U" in link and
        "challenge.size() != kDisarmChallengeLength" in link and
        "(ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f')" in link,
        "disarm challenge must be exactly 32 lowercase hexadecimal characters")
require("difference |=" in link and "return difference == 0;" in link,
        "disarm challenge comparison must not use ordinary early-exit string equality")
disarm_target = link.find('else if (command == "security.disarm") target =')
disarm_persist = link.find("persist_command_replay_state(command_counter, request_id)", disarm_target)
disarm_consume = link.find("consume_disarm_challenge()", disarm_target)
disarm_side_effect = link.find("model_->set_partition_arm", disarm_target)
require(disarm_target >= 0 and disarm_persist >= 0 and disarm_consume >= 0 and disarm_side_effect >= 0
        and disarm_persist < disarm_consume < disarm_side_effect,
        "disarm must durably persist replay state before burning challenge and applying side effect")
require("canonical_text_safe" in link and 'publish_response(false, "invalid_canonical_text")' in link,
        "signed envelope text fields are not protected from canonical transcript control-character injection")
require("!canonical_text_safe(envelope_device_id)" in link and
        "!canonical_text_safe(request_id)" in link and
        "!canonical_text_safe(actor)" in link and
        "!canonical_text_safe(command)" in link and
        "!canonical_text_safe(challenge)" in link,
        "all signed textual envelope fields must be canonical-text validated")
require('command == "security.disarm_challenge" ? std::string_view{"security.disarm"}' in link and
        "access_control_->authorize_session(actor, authorization_command)" in link,
        "disarm challenge issuance must inherit the security.disarm permission")
require('authorize_session(actor, "security.disarm")' not in link[challenge_branch:challenge_issue],
        "disarm challenge branch must not perform a redundant second authorization")
require('parse_json_string(body, "credential"' not in link and "authorize(actor, credential" not in link,
        "MQTT command path must not transport or re-check a user PIN credential")
require("model_->set_partition_arm" in link and "bus_->dispatch_all" in link, "MQTT security command does not reach live model")
require("deferred to safe command router" not in link, "old deferred MQTT command placeholder remains")
require("nvs_set_str" in nvs and "nvs_get_str" in nvs and "nvs_commit" in nvs, "cloud credentials are not persisted in NVS")
require("enabled > 1 || (enabled == 1 && config.broker_uri.empty())" in nvs and
        "return ESP_ERR_INVALID_STATE;" in nvs,
        "persisted MQTT config must fail closed on invalid enabled state or missing broker")


canonical_fields = [
    '"version"', '"deviceId"', '"requestId"', '"actor"', '"command"', '"keyEpoch"',
    '"counter"', '"issuedAtMs"', '"expiresAtMs"', '"challenge"', '"signature"',
]
for field in canonical_fields:
    require(field in contract, f"canonical MQTT contract missing {field}")
require("Factory identity" in contract and "MQTT credentials" in contract and "Cloud Command Trust identity" in contract,
        "MQTT trust identities are not explicitly separated")
require("firmware enforces signed-command verification" in contract.lower() and
        "guarded by ci" in contract.lower(),
        "contract must state the implemented MQTT security baseline and CI enforcement")

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
print(" - signed command keyEpoch bound to active monotonic Cloud Command Trust version")
print(" - serialized replay admission: lock -> read/validate -> durable commit -> unlock")
print(" - replay-protected one-time disarm challenge issuance + durable burn ordering")
print(" - canonical signed-command contract + trust separation")
