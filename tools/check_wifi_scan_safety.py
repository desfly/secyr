from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
NETWORK = ROOT / "firmware" / "esp-idf" / "main" / "hg_network_http.cpp"
source = NETWORK.read_text(encoding="utf-8")

match = re.search(
    r"std::string\s+NetworkHttp::scan_json\(\)\s+const\s*\{(?P<body>.*?)\n\}",
    source,
    flags=re.S,
)
if match is None:
    raise SystemExit("FAIL: NetworkHttp::scan_json() not found")

body = match.group("body")
checks = {
    "scan never disconnects STA": "esp_wifi_disconnect" not in body,
    "scan never forces reconnect": "esp_wifi_connect" not in body,
    "scan uses explicit bounded config": "wifi_scan_config_t scan_config" in body,
    "scan is active": "WIFI_SCAN_TYPE_ACTIVE" in body,
    "active scan dwell is bounded": "scan_config.scan_time.active.max = 60" in body,
    "scan preserves result limit": "kMaxScanRecords" in body,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(("OK   " if ok else "FAIL ") + name)

if failed:
    raise SystemExit("Wi-Fi scan safety contract failed: " + ", ".join(failed))

print("Wi-Fi scan safety contract PASS")
