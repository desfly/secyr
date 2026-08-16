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


def mobile_web_runtime_smoke() -> None:
    """Exercise the actual Web UI at a phone-sized viewport.

    Static string checks previously let a conflicting firmware/mobile CSS rule pass.
    This gate asks Chrome for the final computed layout and executes the exact
    duplicate-href navigation interactions that failed in the field:
    Zones/Sensors, Inputs/Outputs and Settings/System must never produce two
    active items, even when the hash itself does not change.
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
  const activeCount = () => document.querySelectorAll('.sidebar nav a.active').length;
  const activeIs = link => link && link.classList.contains('active') && activeCount() === 1;
  const openMenu = () => {
    if (!sidebar.classList.contains('mobile-menu-open')) toggle.click();
    return getComputedStyle(nav).display === 'grid';
  };
  const clickDuplicate = (href, index) => {
    const links = [...document.querySelectorAll(`.sidebar nav a[href="${href}"]`)];
    if (links.length < 2 || !openMenu()) return false;
    links[index].click();
    // This check is intentionally immediate: no timeout/microtask is allowed
    // to be required to repair Build-948's double-active flash.
    return activeIs(links[index]);
  };

  let ok = !!(sidebar && bruce && image && nav && toggle);
  if (ok) {
    const br = bruce.getBoundingClientRect();
    const tr = toggle.getBoundingClientRect();
    ok = window.matchMedia('(max-width:760px)').matches &&
         getComputedStyle(image).objectFit === 'contain' &&
         getComputedStyle(nav).display === 'none' &&
         getComputedStyle(sidebar).position !== 'fixed' &&
         br.height >= 120 && tr.top >= br.bottom - 1 && activeCount() === 1;
  }

  if (ok) {
    ok = openMenu();
    const nr = nav.getBoundingClientRect();
    const tr2 = toggle.getBoundingClientRect();
    ok = ok && getComputedStyle(nav).position === 'static' &&
         nr.top >= tr2.bottom - 1 && bruce.getBoundingClientRect().height >= 120;
  }

  if (ok) ok = clickDuplicate('#zones-section', 0); // Зони
  if (ok) ok = clickDuplicate('#zones-section', 1); // Датчики, same hash
  if (ok) ok = clickDuplicate('#io-section', 0);    // Входи
  if (ok) ok = clickDuplicate('#io-section', 1);    // Виходи, same hash
  if (ok) ok = clickDuplicate('#system', 0);        // Налаштування
  if (ok) ok = clickDuplicate('#system', 1);        // Система, same hash

  // Every mobile link click must collapse the menu again and Bruce must remain
  // fully in normal document flow rather than being covered by navigation.
  if (ok) {
    ok = !sidebar.classList.contains('mobile-menu-open') &&
         getComputedStyle(nav).display === 'none' &&
         bruce.getBoundingClientRect().height >= 120 &&
         activeCount() === 1;
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
                    "--virtual-time-budget=5000",
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
                errors.append("mobile Web UI smoke: computed phone layout/navigation failed")
            else:
                print("Mobile Web UI runtime smoke PASS (390x844; duplicate nav pairs exercised)")
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
