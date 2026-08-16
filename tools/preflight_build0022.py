from pathlib import Path
import csv
import re
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
ESP = ROOT / "firmware" / "esp-idf"
MAIN = ESP / "main"
WEB = ROOT / "web"

errors = []
warnings = []

required = [
    ESP / "CMakeLists.txt",
    ESP / "sdkconfig.defaults",
    ESP / "partitions.csv",
    MAIN / "CMakeLists.txt",
    MAIN / "app_main.cpp",
    MAIN / "hg_version.hpp",
]

for path in required:
    if not path.exists():
        errors.append(f"missing: {path.relative_to(ROOT)}")

partition_rows = []
with (ESP / "partitions.csv").open(encoding="utf-8") as f:
    for row in csv.reader(line for line in f if not line.lstrip().startswith("#")):
        if row and any(cell.strip() for cell in row):
            partition_rows.append([cell.strip() for cell in row])

names = [row[0] for row in partition_rows]
for name in ["nvs", "otadata", "factory", "ota_0", "ota_1", "storage"]:
    if name not in names:
        errors.append(f"partition missing: {name}")

# Check offsets/sizes do not exceed 16 MiB when numeric.
limit = 16 * 1024 * 1024
for row in partition_rows:
    if len(row) < 5:
        errors.append(f"invalid partition row: {row}")
        continue
    try:
        offset = int(row[3], 0)
        size = int(row[4], 0)
        if offset + size > limit:
            errors.append(f"partition exceeds 16 MiB: {row[0]}")
    except ValueError:
        warnings.append(f"non-numeric partition value: {row[0]}")

sdk = (ESP / "sdkconfig.defaults").read_text(encoding="utf-8")
required_sdk = [
    "CONFIG_IDF_TARGET=\"esp32s3\"",
    "CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y",
    "CONFIG_SPIRAM_MODE_OCT=y",
    "CONFIG_PARTITION_TABLE_CUSTOM=y",
]
for item in required_sdk:
    if item not in sdk:
        errors.append(f"sdkconfig missing: {item}")

cmake = (MAIN / "CMakeLists.txt").read_text(encoding="utf-8")
source_refs = re.findall(r'"([^"]+\.(?:cpp|c))"', cmake)
for ref in source_refs:
    source = (MAIN / ref).resolve()
    if not source.exists():
        errors.append(f"CMake source missing: {ref}")

# Detect duplicate source references.
seen = set()
for ref in source_refs:
    if ref in seen:
        errors.append(f"duplicate CMake source: {ref}")
    seen.add(ref)

# Headers that use ESP_RETURN_ON_ERROR must include esp_check.h in corresponding cpp.
for cpp in MAIN.glob("*.cpp"):
    text = cpp.read_text(encoding="utf-8")
    if "ESP_RETURN_ON_ERROR" in text and '#include "esp_check.h"' not in text:
        errors.append(f"esp_check.h missing: {cpp.name}")


# Cemented field-runtime contract from the 2026-08-16 Build-877 hardware test.
def source_text(name: str) -> str:
    path = MAIN / name
    if not path.is_file():
        errors.append(f"field runtime contract missing source: {name}")
        return ""
    return path.read_text(encoding="utf-8")


def require_token(text: str, token: str, label: str) -> None:
    if token not in text:
        errors.append(f"field runtime contract regressed: {label}")


network = source_text("hg_network_http.cpp")
i2c = source_text("hg_i2c_bus.cpp")
ads = source_text("hg_ads1115.cpp")
ina = source_text("hg_ina226.cpp")
mcp = source_text("hg_mcp23017.cpp")
rtc = source_text("hg_ds3231.cpp")
w5500 = source_text("hg_w5500.cpp")
sd = source_text("hg_sd_storage.cpp")
telemetry = source_text("hg_telemetry_runtime.cpp")
bootstrap = source_text("hg_hardware_bootstrap.cpp")
service = source_text("hg_service_http.cpp")
app_main = source_text("app_main.cpp")

# 1: Wi-Fi reconnect/recovery must be event-driven; status GET must be side-effect free;
# scanning must not deliberately disconnect an established STA. Reconnect retries use
# a bounded one-shot timer so the event loop is never blocked and a dead AP cannot
# create a tight reconnect storm.
require_token(network, "WIFI_EVENT_STA_DISCONNECTED", "Wi-Fi disconnect event reconnect")
require_token(network, "IP_EVENT_STA_GOT_IP", "Wi-Fi recovery AP retirement after got-IP")
require_token(network, "esp_timer_start_once", "Wi-Fi one-shot reconnect timer")
require_token(network, "kReconnectMaximumUs", "Wi-Fi bounded reconnect backoff")
require_token(network, "esp_netif_destroy_default_wifi", "Wi-Fi partial-start netif rollback")
status_start = network.find("esp_err_t NetworkHttp::handle_status")
status_end = network.find("esp_err_t NetworkHttp::handle_scan", status_start)
if status_start < 0 or status_end < 0 or "esp_wifi_set_mode" in network[status_start:status_end]:
    errors.append("field runtime contract regressed: network status GET changed Wi-Fi mode")
scan_start = network.find("std::string NetworkHttp::scan_json")
if scan_start < 0 or "esp_wifi_disconnect" in network[scan_start:]:
    errors.append("field runtime contract regressed: Wi-Fi scan disconnects STA")
wifi_event_start = network.find("void NetworkHttp::handle_wifi_event")
wifi_event_end = network.find("void NetworkHttp::handle_ip_event", wifi_event_start)
if wifi_event_start < 0 or wifi_event_end < 0 or "esp_wifi_connect()" in network[wifi_event_start:wifi_event_end]:
    errors.append("field runtime contract regressed: Wi-Fi disconnect callback performs immediate reconnect")

# 2 + 3: W5500 lifecycle and ISR ordering.
for token, label in [
    ("gpio_install_isr_service", "W5500 GPIO ISR service installation"),
    ("esp_eth_driver_uninstall", "W5500 Ethernet driver rollback"),
    ("esp_eth_del_netif_glue", "W5500 netif glue rollback"),
    ("phy_->del", "W5500 PHY rollback"),
    ("mac_->del", "W5500 MAC rollback"),
    ("spi_bus_free", "W5500 SPI rollback"),
    ("if (uninstall_error == ESP_OK)", "W5500 uninstall ownership guard"),
    ("if (eth_ == nullptr)", "W5500 lower-resource ownership guard"),
]:
    require_token(w5500, token, label)

# 4: INA226 must not remain logically Ready after configuration failure or log every sample error.
require_token(ina, "initialized_ = true", "INA226 transactional initialization")
require_token(ina, "remove_device(&device_)", "INA226 failed-init cleanup")
if "ESP_RETURN_ON_ERROR" in ina:
    errors.append("field runtime contract regressed: INA226 per-read ESP_ERROR logging restored")

# 5: physical I2C ACK is mandatory before accepting a device handle.
require_token(i2c, "i2c_master_probe", "I2C physical ACK probe")

# ADS1115 and DS3231 must not equate a software I2C handle with a live device.
for text, label in [
    (ads, "ADS1115"),
    (rtc, "DS3231"),
]:
    require_token(text, "initialized_ = true", f"{label} transactional initialization")
    require_token(text, "remove_device(&device_)", f"{label} failed-init cleanup")
    require_token(text, "initialized_ && device_ != nullptr", f"{label} strict ready state")

# 6: MCP23017 initialization is transactional and safe outputs are part of successful init.
require_token(mcp, "remove_device(&device_)", "MCP23017 failed-init cleanup")
require_token(mcp, "kOlatA, 0x00", "MCP23017 safe-output initialization")

# 7: degraded bootstrap must be visible instead of a false all-good completion.
require_token(app_main, "Hardware bootstrap completed DEGRADED", "degraded hardware bootstrap reporting")
require_token(bootstrap, "HardwareModuleState::Fault", "hardware fault classification")

# 8: absent SD card releases owned SPI resources and is not polled when unmounted.
require_token(sd, "spi_bus_free(SPI3_HOST)", "microSD failed-mount SPI cleanup")
require_token(telemetry, "storage().status().mounted", "microSD runtime gating")

# 9: telemetry respects bootstrap hardware state and does not hammer optional buses.
require_token(telemetry, "hardware_status.ina226.state", "INA226 telemetry gating")
require_token(telemetry, "hardware_status.ads1115_telemetry.state", "ADC telemetry gating")
require_token(telemetry, "hardware_status.ds3231.state", "RTC telemetry gating")
require_token(telemetry, "kOneWireRediscoveryCycles", "1-Wire rediscovery backoff")
require_token(telemetry, "rediscovery_due", "1-Wire absent-device polling gate")

# 10: a true factory reset is different from service commissioning invalidation.
for token, label in [
    ('/api/v1/service/factory-reset', "factory reset endpoint"),
    ('confirmation != "ERASE_ALL"', "factory reset explicit confirmation"),
    ("nvs_flash_erase()", "factory reset whole-NVS erase"),
    ("esp_restart()", "factory reset reboot"),
    ('"factoryReset\\\":true', "factory reset response marker"),
]:
    require_token(service, token, label)

# 10 + 12: clean-state and fail-closed rules are persistent project contracts.
field_contract = ROOT / "docs" / "FIELD_TEST_BUGS_2026-08-16.md"
if not field_contract.is_file():
    errors.append("field runtime contract missing: docs/FIELD_TEST_BUGS_2026-08-16.md")
else:
    field_text = field_contract.read_text(encoding="utf-8")
    require_token(field_text, "Clean-state test / Factory Reset invariant", "clean-state Factory Reset invariant")
    require_token(field_text, "FAIL-CLOSED physical outputs", "physical-output fail-closed invariant")
require_token(app_main, "Physical outputs remain FAIL-CLOSED after boot", "boot fail-closed output gate")

# 11: preserve ESP-IDF's normal thread-safe logger.
if "esp_log_set_vprintf(esp_rom_vprintf)" in app_main:
    errors.append("field runtime contract regressed: non-reentrant ROM vprintf override restored")


def mobile_web_runtime_smoke() -> None:
    """Exercise the actual Web UI at a phone-sized viewport.

    Static string checks previously let a conflicting firmware/mobile CSS rule pass.
    This gate asks Chrome for the final computed layout: Bruce must use contain,
    navigation must be collapsed by default, expanding it must stay in document
    flow below Bruce/toggle, and exactly one sidebar item may be active.
    """
    chrome = shutil.which("google-chrome") or shutil.which("chromium") or shutil.which("chromium-browser")
    if not chrome:
        errors.append("mobile Web UI smoke: Chrome/Chromium not found")
        return
    if not (WEB / "index.html").is_file():
        errors.append("mobile Web UI smoke: web/index.html missing")
        return

    probe = r"""
<script>
setTimeout(() => {
  const sidebar = document.querySelector('.sidebar');
  const bruce = document.querySelector('.sidebar .bruce');
  const image = document.querySelector('.sidebar .bruce img');
  const nav = document.querySelector('.sidebar nav');
  const toggle = document.querySelector('#mobileMenuToggle');
  const activeCount = document.querySelectorAll('.sidebar nav a.active').length;
  let ok = !!(sidebar && bruce && image && nav && toggle);
  if (ok) {
    const br = bruce.getBoundingClientRect();
    const tr = toggle.getBoundingClientRect();
    ok = window.matchMedia('(max-width:760px)').matches &&
         getComputedStyle(image).objectFit === 'contain' &&
         getComputedStyle(nav).display === 'none' &&
         getComputedStyle(sidebar).position !== 'fixed' &&
         br.height >= 120 && tr.top >= br.bottom - 1 && activeCount === 1;
    if (ok) {
      toggle.click();
      const nr = nav.getBoundingClientRect();
      const tr2 = toggle.getBoundingClientRect();
      ok = getComputedStyle(nav).display === 'grid' &&
           getComputedStyle(nav).position === 'static' &&
           nr.top >= tr2.bottom - 1 && bruce.getBoundingClientRect().height >= 120;
    }
  }
  document.documentElement.dataset.mobileLayoutSmoke = ok ? 'pass' : 'fail';
}, 700);
</script>
"""

    with tempfile.TemporaryDirectory(prefix="homeguard-mobile-") as tmp:
        root = Path(tmp) / "web"
        shutil.copytree(WEB, root)
        index = root / "index.html"
        html = index.read_text(encoding="utf-8")
        if "</body>" not in html:
            errors.append("mobile Web UI smoke: index.html has no </body>")
            return
        index.write_text(html.replace("</body>", probe + "\n</body>", 1), encoding="utf-8")
        port = 18765
        server = subprocess.Popen(
            [sys.executable, "-m", "http.server", str(port), "--directory", str(root)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        try:
            time.sleep(0.7)
            result = subprocess.run(
                [
                    chrome,
                    "--headless",
                    "--no-sandbox",
                    "--disable-gpu",
                    "--window-size=390,844",
                    "--virtual-time-budget=4000",
                    "--dump-dom",
                    f"http://127.0.0.1:{port}/",
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                timeout=20,
            )
            if result.returncode != 0:
                errors.append(f"mobile Web UI smoke: Chrome exit {result.returncode}")
            elif 'data-mobile-layout-smoke="pass"' not in result.stdout:
                errors.append("mobile Web UI smoke: computed phone layout failed")
            else:
                print("Mobile Web UI runtime smoke PASS (390x844)")
        except subprocess.TimeoutExpired:
            errors.append("mobile Web UI smoke: Chrome timed out")
        finally:
            server.terminate()
            try:
                server.wait(timeout=3)
            except subprocess.TimeoutExpired:
                server.kill()


mobile_web_runtime_smoke()

for message in warnings:
    print(f"WARNING: {message}")

if errors:
    for message in errors:
        print(f"ERROR: {message}")
    sys.exit(1)

print("Build-0022 preflight PASS")
