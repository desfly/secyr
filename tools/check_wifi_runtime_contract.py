from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
MAIN = ROOT / "firmware" / "esp-idf" / "main"
WEB = ROOT / "web"
SDK = ROOT / "firmware" / "esp-idf" / "sdkconfig.defaults"

errors = []
network = (MAIN / "hg_network_http.cpp").read_text(encoding="utf-8")
header = (MAIN / "hg_network_http.hpp").read_text(encoding="utf-8")
web_http = (MAIN / "hg_web_http.cpp").read_text(encoding="utf-8")
web_app = (WEB / "app.js").read_text(encoding="utf-8")
sdk = SDK.read_text(encoding="utf-8")
app_main = (MAIN / "app_main.cpp").read_text(encoding="utf-8")

# A blocking scan must never execute on the HTTPD task. The async request is
# explicitly completed on every worker path through an RAII guard.
for snippet in (
    "httpd_req_async_handler_begin",
    "class AsyncRequestGuard",
    "httpd_req_async_handler_complete",
    "dispatch_async(request, AsyncOperation::Scan)",
    "dispatch_async(request, AsyncOperation::Connect",
):
    if snippet not in network:
        errors.append(f"network runtime missing async socket contract: {snippet}")

scan_handler_start = network.find("esp_err_t NetworkHttp::handle_scan")
scan_worker_start = network.find("void NetworkHttp::process_scan")
scan_call = network.find("scan_json()", scan_handler_start)
if scan_call >= 0 and scan_worker_start >= 0 and scan_call < scan_worker_start:
    errors.append("blocking scan_json must not run directly from handle_scan")

# The HTTP acknowledgement must be sent and async ownership released before
# the live STA socket transport is intentionally dropped.
process_start = network.find("void NetworkHttp::process_connect")
process_end = network.find("bool NetworkHttp::start_candidate_timeout", process_start)
process = network[process_start:process_end] if process_start >= 0 and process_end > process_start else ""
positions = {
    "candidate": process.find("save_candidate_credentials(context.ssid, context.password)"),
    "response": process.find("const auto response_error = send_json"),
    "complete": process.find("const auto complete_error = request_guard.complete()"),
    "delay": process.find("vTaskDelay(kStaHandoverDelay)"),
    "disconnect": process.find("esp_wifi_disconnect()"),
    "set_config": process.find("set_sta_config(context.ssid, context.password)"),
    "connect": process.find("const auto connect_error = esp_wifi_connect()"),
}
if min(positions.values(), default=-1) < 0 or list(positions.values()) != sorted(positions.values()):
    errors.append("Wi-Fi handover order must be candidate -> HTTP response -> async complete -> delay -> disconnect -> config -> connect")
if "save_credentials(" in process or "clear_credentials()" in process:
    errors.append("process_connect must not mutate committed credentials before verified GOT_IP")

# A stale GOT_IP from the old AP must not be able to promote a candidate.
ip_start = network.find("void NetworkHttp::on_ip_event")
ip_end = network.find("bool NetworkHttp::set_sta_config", ip_start)
ip_handler = network[ip_start:ip_end] if ip_start >= 0 and ip_end > ip_start else ""
match_pos = ip_handler.find("current_ap_matches(candidate_ssid)")
commit_pos = ip_handler.find("save_credentials(candidate_ssid, candidate_password)")
if match_pos < 0 or commit_pos < 0 or match_pos > commit_pos:
    errors.append("candidate GOT_IP must validate the associated SSID before commit")
if "compare_exchange_strong(expected, false)" not in ip_handler:
    errors.append("candidate promotion must claim the pending state atomically")

# Delayed recovery must re-apply committed credentials, not trust transient
# RAM configuration left by a failed candidate.
retry_start = network.find("void NetworkHttp::reconnect_task_entry")
retry_end = network.find("void NetworkHttp::candidate_timeout_task_entry", retry_start)
retry = network[retry_start:retry_end] if retry_start >= 0 and retry_end > retry_start else ""
if "restore_active_connection()" not in retry:
    errors.append("delayed reconnect must restore committed configuration")

# The application deliberately uses RAM-only ESP Wi-Fi config; HomeGuard NVS
# is the single source of truth for active/candidate transaction records.
if "esp_wifi_set_storage(WIFI_STORAGE_RAM)" not in network:
    errors.append("ESP Wi-Fi storage must remain RAM-only")
if "kCandidateNvsKey" not in network or "clear_candidate_credentials()" not in network:
    errors.append("candidate transaction record lifecycle is incomplete")

# Browser/API load must be bounded: API fetches are serialized and timed out,
# while firmware asset delivery is streamed without a full-size temporary copy.
for snippet in ("apiQueueTail", "AbortController", "apiTimeoutMs"):
    if snippet not in web_app:
        errors.append(f"Web API socket bound missing: {snippet}")
if "httpd_resp_send_chunk" not in web_http:
    errors.append("firmware text assets must use chunked streaming")
if "std::string body(reinterpret_cast<const char*>(start)" in web_http:
    errors.append("firmware Web assets must not be copied into a full temporary std::string")
if "installWifiConnectHandoverFetchGuard" in web_http or "new Response(" in web_http:
    errors.append("firmware Web layer must not synthesize Wi-Fi connect success")

socket_match = re.search(r"^CONFIG_LWIP_MAX_SOCKETS=(\d+)$", sdk, flags=re.MULTILINE)
if not socket_match:
    errors.append("CONFIG_LWIP_MAX_SOCKETS must be explicit")
elif int(socket_match.group(1)) < 16:
    errors.append("LWIP socket budget must be at least 16")
if "config.lru_purge_enable = true;" not in app_main:
    errors.append("HTTPD LRU purge must remain enabled")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print("Wi-Fi / HTTP socket stability contract PASS")
print(" - scan/connect work is off the HTTPD task and async requests are completed")
print(" - handover acknowledges HTTP before intentional STA disconnect")
print(" - candidate credentials commit only after matching-SSID GOT_IP")
print(" - delayed reconnect restores committed NVS credentials")
print(" - Web API concurrency/timeouts and LWIP/HTTPD socket budget are bounded")
