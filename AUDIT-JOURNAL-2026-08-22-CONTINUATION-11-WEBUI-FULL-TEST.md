# HomeGuard-S3 audit journal — continuation 11: full Web UI hardware test

Date: 2026-08-22 (Europe/Kyiv)
Firmware under hardware test: HomeGuard-S3 Build #1876
Verified local `homeguard_s3.bin` SHA256: `b0cc3055d727e69e6713ca6d278676d36ea66e8648119c9b26894a9a7a67298f`

## Hardware checkpoint before Web UI

User reports the complete physical reset/RGB sequence has already been observed on video and is OK:

`WHITE -> WHITE -> WHITE -> RED`

RST/RGB is therefore closed for this Web UI test. Do not modify the RGB driver or reset detector while Web UI validation is in progress.

## Full Web UI validation scope

Test the real controller/browser path, not only CI mocks.

1. First-boot/setup gate on controller AP (`192.168.4.1` after Factory Reset).
2. Desktop layout at browser zoom 100%: Bruce artwork, setup card size/typography, no clipping/overlap.
3. Setup Wi-Fi scan and AP rendering.
4. SSID selection, Wi-Fi password entry/visibility control if present, connect handover.
5. Verify controller becomes reachable through infrastructure STA and setup remains available until first Admin is created.
6. Create first Admin and verify setup closes.
7. Login gate: correct layout, invalid PIN rejection, valid login, logout, re-login.
8. Main dashboard: online state, clock/date, security state, zones, Wi-Fi, cloud card, quick actions, events/history, I/O.
9. Navigation: Panel, Zones, Sensors, Inputs, Outputs, Events, History, Network, System.
10. Network page: state/SSID/IP, Wi-Fi scan, network selection/connect, LAN scan/device list.
11. Role/access UI: Admin-only controls and user capability restrictions.
12. System/Cloud section: build information, MQTT form/status; do not overwrite real cloud credentials unless explicitly intended during test.
13. Security commands and output controls only where hardware-safe; verify UI response and API authorization.
14. Responsive/mobile browser layout after desktop pass.
15. Factory-reset-from-Web test LAST, because it intentionally destroys mutable state; verify setup returns after reboot.

## Evidence discipline

For every stage record PASS/FAIL from real browser screenshots/video and controller behavior. Do not infer success from CI alone. When a defect is found, freeze the test at that point, identify the exact UI/API contract failure, make one minimal change, rebuild, verify artifact SHA, then resume from the recorded checkpoint.

## Checkpoint 1 — first-boot setup page on real controller

Evidence: user screenshot from Chrome at `http://192.168.4.1`, desktop viewport, 2026-08-22.

Observed PASS:
- `Первинне налаштування` gate renders on the controller AP.
- Bruce artwork renders on the right and is not clipped by the setup card.
- Two-column setup card is fully visible with no overlap between network and first-Admin panels.
- Main heading, explanatory text, labels, fields and action buttons are readable and proportionally consistent.
- No horizontal clipping is visible; the full setup card fits inside the viewport.
- Wi-Fi and first-Admin controls are present.

Observed FAIL / regression:
- Password visibility eye controls were absent from BOTH `Пароль Wi-Fi` and `Пароль / PIN` fields on the setup page.
- Current setup form uses dynamically inserted password inputs, while the existing embedded eye-control only targeted the main Network-page `#wifiPassword` field.

### Fix applied

Commit `7bd216bdb9a98c20013703b0518de8ef15ffaa93` — `fix(web): restore setup password eye controls`.

Minimal embedded-browser change in `firmware/esp-idf/main/hg_web_http.cpp`:
- generalizes the already proven main Wi-Fi eye-control helper;
- adds compact eye toggle for dynamic setup `#hgSetupWifiPassword`;
- adds compact eye toggle for dynamic setup `#hgSetupPin`;
- uses a `MutationObserver` because these setup fields are created later by `access-session.js`;
- reserves only 52 px inside each password input for the eye; setup card width, grid, typography and overall geometry are unchanged;
- toggles `password <-> text`, updates aria state/title, then returns focus to the field.

Commit `19963e81bd31dff82851dd4c2896f3138dbd8003` — `test(web): lock setup password eye controls`.

`tools/check_setup_ui_contract.py` now fails CI if either setup eye-control, the dynamic observer, or right-side input reservation disappears again.

Checkpoint status: LAYOUT PASS; PASSWORD-VISIBILITY DEFECT FIXED IN SOURCE; HARDWARE RECHECK PENDING NEW GREEN BUILD.

Resume point after new artifact: flash verified build, return to `192.168.4.1`, confirm both setup eye controls visually and functionally, then continue with Wi-Fi scan/AP rendering.
