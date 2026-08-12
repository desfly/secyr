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
cmake = read("CMakeLists.txt")
errors = []

def require(ok: bool, msg: str):
    if not ok: errors.append(msg)

require('"hg_cloud_nvs.cpp"' in cmake, "cloud NVS source not compiled")
require("g_cloud_store.load" in app and "restore_cloud_config" in app, "cloud config not restored at boot")
require("g_cloud_link.start" in app, "persisted cloud config does not auto-start MQTT")
require("set_command_runtime(&g_system_model, &g_system_bus, &g_access_control)" in app, "live command runtime not wired")
require('"/api/v1/cloud/status"' in http, "cloud status endpoint missing")
require('"/api/v1/cloud/config"' in http, "cloud config endpoint missing")
require('authorize(actor, credential, "cloud.configure")' in http, "cloud config endpoint not access-controlled")
require("store_->save(config)" in http, "cloud config is not persisted")
require("cloud_->stop()" in http and "cloud_->start(" in http, "cloud config change does not restart MQTT")
require("responses" in link and "response_topic_" in link, "MQTT response topic missing")
require("handle_command(event->data" in link, "MQTT command payload is not routed")
require("access_control_->authorize(actor, credential, command)" in link, "MQTT commands bypass AccessControl")
require("model_->set_partition_arm" in link and "bus_->dispatch_all" in link, "MQTT security command does not reach live model")
require("deferred to safe command router" not in link, "old deferred MQTT command placeholder remains")
require("nvs_set_str" in nvs and "nvs_get_str" in nvs and "nvs_commit" in nvs, "cloud credentials are not persisted in NVS")

if errors:
    print("Cloud runtime contract FAIL")
    for error in errors:
        print(" -", error)
    sys.exit(1)

print("Cloud runtime contract PASS")
print(" - persistent MQTT config + boot restore")
print(" - admin-only cloud configuration endpoint")
print(" - live AccessControl/SystemModel command routing")
print(" - MQTT responses topic")
