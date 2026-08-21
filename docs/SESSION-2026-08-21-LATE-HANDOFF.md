# HomeGuard-S3 — late-session handoff, 2026-08-21

This file is the external memory checkpoint for the next session. Read it before changing code. Do not re-solve already tested items from scratch.

## Repository state
- Repository: `desfly/secyr`
- Active PR: #72
- Branch: `fix/setup-ui-clean-20260821`
- Current branch head before this handoff file: `8fce58f89ef1e3e586d47028e2589233e1f637f1`
- Base: `main` at `b83157e60311740be9db1e0797642511876b3e39`

## Physical RST / RGB — verified behavior and late observation
- The 3-step Settings Reset / 5-step Factory Reset contract had already passed hardware validation on Build #1837.
- New-firmware post-flash suppression is based on `hg_rstseq/fw_sig` derived from `HG_GIT_REVISION` and must remain in place.
- Build #1844 was flashed successfully on the real ESP32-S3 board through COM6 using the established offsets.
- Observed `homeguard_s3.bin` write size for Build #1844: **1,801,872 bytes**.
- Every flashed segment reported `Hash of data verified`.
- After Build #1844 finished and esptool printed `Hard resetting via RTS pin...`, the user observed **NO short WHITE**. This is the expected result and confirms the new-firmware `fw_sig` baseline suppression still works after the UI-only change.
- Earlier, re-flashing the same Build #1843 image produced a short WHITE after the esptool reset. Treat that as an unresolved same-firmware edge case, not as a reason to change the already hardware-verified 3/5 reset contract without evidence. Do not touch RST/RGB code unless explicitly requested.

## Web UI visual decisions from hardware/browser screenshots
### Login window
- The user explicitly approved **window #1 (Login)**.
- Do not redesign or resize it unless the user asks.

### First-boot setup window
- The approved visual baseline is the restored large dark setup card on the left with large Bruce on the right.
- The setup card is two-column on desktop: Wi-Fi on the left, first Admin on the right.
- Do not return to the later compact/top-left experimental layout.

## Wi-Fi scan list — fixed and hardware/browser verified
Problem:
- Restoring the old approved setup appearance also restored the old inner Wi-Fi list scrollbar via `max-height`/`overflow`.

Fix commit:
- `8fce58f89ef1e3e586d47028e2589233e1f637f1` — `Restore full Wi-Fi scan list without inner scrolling`

Exact intended desktop behavior:
- `app.css` overrides the injected access-session default so `.hg-setup-networks` has `max-height:none!important; overflow:visible!important` on desktop.
- The setup stage allows the expanded list height.
- The small Wi-Fi results box must **not** have its own scrollbar.
- If many networks are discovered, the **whole page** scrolls instead.

Hardware/browser result on Build #1844:
- User scanned Wi-Fi and got **20 networks**.
- All results were visible in the expanded list.
- No inner Wi-Fi scrollbar was present.
- Selecting `Nitros` filled the SSID field and password entry worked.
- Whole-page browser scrolling handled the long page correctly.
- Therefore the no-inner-scroll Wi-Fi behavior is **hardware/browser PASS**.

## CI state for head `8fce58f...` / Build #1844
Successful:
- Setup UI Contract #75 — SUCCESS.
- Web UI Preview #470 — SUCCESS.
- HomeGuard Wi-Fi Stability #47 — SUCCESS.
- HomeGuard-S3 Build #1844 — SUCCESS.

Build artifact:
- Artifact name: `HomeGuard-S3-firmware`
- Artifact ID: `9461149526`
- Artifact ZIP size: `9,940,085` bytes
- Artifact digest: `sha256:50dd3c0db18b25e1da4d01b8987aa6212300dd0f1a96f0ebf9308fe41ab131ba`

Known CI noise/failure:
- Web Navigation Runtime Audit #79 — FAILURE.
- Web Navigation Audit #64 — FAILURE.
- Static Web UI contract passed before the browser step.
- Failure reason in the browser runtime step was cleanup only: `Directory not empty: '/tmp/homeguard-nav-chrome-.../Default'`.
- Do not treat this as evidence of a Web UI functional regression. Fix the temp Chromium profile cleanup separately if/when requested.
- Do not revert the callback-driven browser navigation probe back to the old multi-Chrome or dump-dom exit strategy.

## Current open UI bug: missing password visibility eye
The latest user complaint is: **"де глазик в паролі ???????????"**

Current code fact:
- `web/access-session.js` currently creates password inputs as plain `<input type="password">` controls.
- There is no password visibility toggle/eye control in the setup/login markup.
- The visible omission is especially obvious in the first-boot Wi-Fi password field shown in the latest screenshot.

Next task for the next session:
- Add a small eye/show-hide control without changing approved layout sizes or reset/network logic.
- Apply consistently to the first-boot/login password fields at minimum:
  - `#hgLoginPin`
  - `#hgSetupWifiPassword`
  - `#hgSetupPin`
- Prefer one reusable helper/toggle behavior rather than three unrelated implementations.
- Toggle only the input `type` between `password` and `text`; do not persist, log, copy, or transmit the secret differently.
- Preserve autocomplete/inputmode semantics.
- Add/extend the UI contract test so these visibility toggles cannot silently disappear again.
- After coding, build a new firmware and hardware/browser-test the eye controls; do not declare PASS before the user confirms on the real UI.

## Safety rails for tomorrow
- Read this file and `WORKLOG.md` first.
- Do not touch the already verified 3×/5× physical reset contract.
- Do not reintroduce an inner Wi-Fi list scrollbar.
- Do not compact or redesign the approved Login window.
- Do not change Bruce placement/asset while fixing the eye control.
- Keep the eye fix narrowly scoped to password visibility + its regression guard.
- After each meaningful code/test result, record exact commit/run/artifact/hardware result in Git or PR #72.
