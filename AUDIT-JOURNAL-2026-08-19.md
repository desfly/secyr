# HomeGuard-S3 Audit Journal — 2026-08-19

Purpose: durable handoff point for continuing this audit from desktop or mobile without losing the exact state.

## Exact repository state at journal creation

- Repository: `desfly/secyr`
- Branch: `main`
- Head before this journal commit: `6b1167ebe595e2788f7bbeb9996ca785e700687c`
- Baseline at start of this audit segment: `5f426f877006b1e8a7fea5cdf82e6cf2f20286eb`
- CI status for the newest commits was not available through the connector at the time of audit. Do **not** call the tree green until Actions confirms it.

## Commits made in this audit segment

1. `1ca982be1589effc484a46b2b1bd0283bad664fb` — **Remove stale embedded Bruce fallback from Web UI**
   - Removed obsolete `data:image/webp;base64,...` Bruce from `web/app.css`.
   - Left the approved `web/bruce.jpg` as the single artwork source.
   - Preserved full-image `object-fit: contain` behavior in source CSS.

2. `73a3a54b844b7b52a3e907234762f4d25101b8fe` — **Fix mobile Bruce crop and serve factory reset asset**
   - Found firmware-injected mobile CSS in `hg_web_http.cpp` overriding the approved portrait with `object-fit:cover!important` and a short 86 px container.
   - Changed firmware mobile Bruce to 180 px + `object-fit:contain!important` + centered positioning.
   - Changed firmware mobile nav to stay collapsed until `.mobile-nav-open`.
   - Added embedded linker symbols and route `/factory-reset.js`.

3. `adc7f034014dbf9f536bea06023a42d115af9321` — **Wire factory reset module into served Web UI**
   - Root cause: `web/factory-reset.js` existed and was embedded by CMake but `index.html` never loaded it, so the System-page Factory Reset UI was dead code.
   - `access_session_js_get()` now appends the embedded factory-reset module to the always-loaded access-session response.
   - `/factory-reset.js` route remains available explicitly as well.

4. `4494e688e530f07674e09c41fd7c9fd24f7ffed1` — **Remove obsolete second navigation active-state controller**
   - Removed old `enforceSingleActive` MutationObserver block from `web/access-session.js`.
   - Reason: navigation hashes are now unique and `app.js::routeFromHash()` must be the single authority assigning `.active`.

5. `07bfe7a7ec39bc2889620f5be98e452f2f714ac7` — **Harden Web UI regression gate for Bruce navigation and reset**
   - Expanded `tools/check_web_ui_contract.py` to check:
     - access-session + factory-reset assets embedded and served;
     - exactly one `.active` assignment authority across loaded JS;
     - obsolete `enforceSingleActive` absent;
     - factory reset calls `/api/v1/system/factory-reset` with `ERASE_ALL` confirmation;
     - firmware factory reset is authorized by `system.factory_reset`;
     - stale base64 Bruce never returns;
     - firmware mobile Bruce remains `contain`, never `cover`;
     - firmware mobile navigation stays collapsed until opened.

6. `6b1167ebe595e2788f7bbeb9996ca785e700687c` — **Fix WS2812 reset latch timing for RGB feedback**
   - Audited `firmware/esp-idf/main/hg_rgb_diagnostic.cpp`.
   - Found the RMT sender emitted only the 24 GRB bits and did not explicitly guarantee the WS2812 post-frame reset/latch LOW interval.
   - Added `tx_config.eot_level = 0` and an 80 us LOW hold via `esp_rom_delay_us()` after `rmt_tx_wait_all_done()`.
   - GPIO was deliberately **not** changed: GPIO48 remains the current confirmed onboard RGB candidate and is reserved from external I/O in `hg_board_hw678.hpp`.

## Confirmed root causes found

### A. Mobile Bruce was being re-cropped by firmware, not by source CSS

`web/app.css` / mobile access-session styling wanted the complete portrait, but `hg_web_http.cpp` appended firmware CSS with `object-fit:cover!important`, which overrode it. That explains why reflashing could still show a cropped Bruce even after source UI fixes.

Status: fixed in `73a3a54b` and protected by regression gate in `07bfe7a7`.

### B. Factory Reset Web UI was embedded but unreachable

`factory-reset.js` was copied into the ESP-IDF component and embedded, and the backend `/api/v1/system/factory-reset` existed, but the browser never loaded the JS module.

Status: fixed in `73a3a54b` + `adc7f034`; gate added in `07bfe7a7`.

### C. Two independent writers were managing menu `.active`

`app.js::routeFromHash()` had already become authoritative, but `access-session.js` still contained an old MutationObserver-based duplicate-active repair path. This created avoidable state races.

Status: removed in `4494e688`; gate added in `07bfe7a7`.

### D. RGB WS2812 transmission did not guarantee the latch/reset interval

The onboard RGB routine used RMT byte encoding for 24 bits but lacked an explicit post-frame LOW reset interval. WS2812-family devices need that LOW period to latch a new color.

Status: fixed in `6b1167eb` with end-of-transmission LOW + 80 us latch delay. Hardware validation still required after a successful firmware build.

## RST / Factory Reset audit state

Files inspected:

- `firmware/esp-idf/main/app_main.cpp`
- `firmware/esp-idf/main/hg_reset_sequence.cpp`
- `firmware/esp-idf/main/hg_rgb_diagnostic.cpp`
- `tests/test_reset_sequence.cpp`
- `firmware/esp-idf/main/hg_board_hw678.hpp`

Current design in `main`:

- NVS initializes first.
- `handle_triple_rst_factory_reset()` runs immediately after NVS and **before** Wi-Fi, HTTP, telemetry, cloud, etc.
- Physical reset candidate accepts both `ESP_RST_EXT` and `ESP_RST_POWERON` classification via the shared reset logic.
- Required sequence is 3 physical resets.
- Counter is persisted in NVS namespace `hg_rstseq`, key `count`.
- Sequence window is 4.5 seconds.
- On third reset:
  1. counter is consumed/cleared;
  2. onboard RGB is commanded WHITE for 5 seconds on GPIO48;
  3. mutable state is erased;
  4. controller restarts.
- Host test `tests/test_reset_sequence.cpp` covers `0 -> 1 -> 2 -> trigger`, non-physical clearing, and POWERON/EXT classification.

Important field symptom from the previous hardware test: three reset-button presses produced no visible reaction. The new WS2812 latch fix is intended to remove one concrete cause of "no LED reaction", but it is **not yet hardware-proven**.

## Important distinction from the old tested firmware

The current source does **not** intentionally show WHITE for 5 seconds on every ordinary boot. WHITE 5 seconds is in the triple-RST factory-reset path. If a flashed build still lights white for 5 seconds on each reboot, that indicates an older/different binary or another RGB initialization path and must be traced from its serial log/build number.

## Pending work — continue exactly here

### 1. Finish the source-audit guard for the new RGB delay API

I had started editing `tools/audit_esp_idf_sources.py` when the handoff request arrived. **That edit was not committed.**

Planned minimal addition:

- if a source uses `esp_rom_delay_us`, require `#include "esp_rom_sys.h"`.

Do not assume this is already in the repository; inspect current file first.

### 2. Verify ESP-IDF 5.4.4 build for `6b1167eb`

Need Actions/ESP-IDF compile confirmation for:

- `rmt_transmit_config_t::eot_level` on ESP-IDF 5.4.4;
- `esp_rom_delay_us()` include/link;
- all Web UI asset symbols/routes after the factory-reset wiring.

Do not claim green until actual CI/check output is visible.

### 3. If build is green, test RST/RGB on real ESP32-S3

Recommended field test after flashing the new artifact:

- open serial at 115200;
- record build number from startup;
- one RST press: expect log `Physical RST sequence: 1/3`;
- second press within 4.5 s: expect `2/3`;
- third press within 4.5 s: expect `Triple RST detected...`, then WHITE RGB ~5 s, then Factory Reset + reboot;
- after reset, confirm Wi-Fi/users/config are erased as intended.

If serial logs count 1/3, 2/3, 3/3 but LED is still dark, isolate RGB hardware/driver next. If the counter never advances, investigate reset-reason classification/NVS instead of the LED.

### 4. Re-check browser/mobile UI after the same firmware build

Verify on phone:

- Bruce fully visible, not cropped;
- menu collapsed by default under Bruce;
- opening menu does not cover Bruce/content;
- only one navigation item blue at a time;
- `Система` page contains Factory Reset panel;
- reset button requires Admin credentials + exact `ERASE_ALL` + browser final confirmation.

## Files intentionally modified during this segment

- `web/app.css`
- `web/access-session.js`
- `firmware/esp-idf/main/hg_web_http.cpp`
- `tools/check_web_ui_contract.py`
- `firmware/esp-idf/main/hg_rgb_diagnostic.cpp`

No broad redesign was intended. Approved Bruce JPEG itself was not replaced.

## Resume phrase

When continuing from another device, say: **"продовжуй аудит з AUDIT-JOURNAL-2026-08-19.md"**.

The next assistant should read this file from `desfly/secyr/main` first, then continue from **Pending work — continue exactly here**.
