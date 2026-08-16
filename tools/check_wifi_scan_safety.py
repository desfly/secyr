from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
NETWORK = ROOT / "firmware" / "esp-idf" / "main" / "hg_network_http.cpp"
source = NETWORK.read_text(encoding="utf-8")

scan_match = re.search(
    r"std::string\s+NetworkHttp::scan_json\(\)\s+const\s*\{(?P<body>.*?)\n\}",
    source,
    flags=re.S,
)
if scan_match is None:
    raise SystemExit("FAIL: NetworkHttp::scan_json() not found")

scan_body = scan_match.group("body")
# Comments explain the Build-877 regression and intentionally mention the old API
# calls. Remove comments before checking executable source so documentation cannot
# create a false failure.
scan_code = re.sub(r"//.*?$", "", scan_body, flags=re.M)
scan_code = re.sub(r"/\*.*?\*/", "", scan_code, flags=re.S)

connect_match = re.search(
    r"esp_err_t\s+NetworkHttp::handle_connect\(httpd_req_t\*\s*request\)\s*\{(?P<body>.*?)\n\}\n\nbool\s+NetworkHttp::apply_sta",
    source,
    flags=re.S,
)
if connect_match is None:
    raise SystemExit("FAIL: NetworkHttp::handle_connect() not found")
connect_code = connect_match.group("body")

checks = {
    "scan never disconnects STA": "esp_wifi_disconnect" not in scan_code,
    "scan never forces reconnect": "esp_wifi_connect" not in scan_code,
    "scan uses explicit bounded config": "wifi_scan_config_t scan_config" in scan_code,
    "scan is active": "WIFI_SCAN_TYPE_ACTIVE" in scan_code,
    "active scan dwell is bounded": "scan_config.scan_time.active.max = 60" in scan_code,
    "scan preserves result limit": "kMaxScanRecords" in scan_code,
    "connect request body is fully read": "read_request_body(request, 384U, body)" in connect_code and "while (offset < body.size())" in source,
    "connect keeps recovery AP during handover": "esp_wifi_set_mode(WIFI_MODE_APSTA)" in connect_code,
    "connect persists accepted credentials": "save_credentials(ssid, password)" in connect_code,
    "connect attempts STA after response write": connect_code.find("send_json(request") < connect_code.find("esp_wifi_connect()"),
    "HTTP write failure cannot skip STA connect": "if (response_error != ESP_OK) return response_error" not in connect_code and "const auto connect_error = esp_wifi_connect();" in connect_code,
    "connect returns socket error only after handover attempt": "return response_error != ESP_OK ? response_error : connect_error;" in connect_code,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(("OK   " if ok else "FAIL ") + name)

if failed:
    raise SystemExit("Wi-Fi safety contract failed: " + ", ".join(failed))

print("Wi-Fi scan/connect safety contract PASS")
