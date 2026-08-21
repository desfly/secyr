# HomeGuard-S3 Worklog / External Memory

This file is the project continuity log. Read it before changing code or re-solving an old problem.

## 2026-08-21 — verified checkpoint

### Rule for future sessions
- Git is the external project memory, not only code storage.
- Before starting work, read this worklog, recent commits, PRs and CI status.
- After a meaningful change, record: what changed, why, what was tested, exact commit/run/artifact, remaining issue and next step.
- Do not re-invent or silently replace an already agreed solution without evidence that it failed.

### Physical RST / RGB reset
- The old GPIO21-based reset interpretation was removed/replaced by the physical RST/EN reset flow.
- Mainline checkpoint containing the first RST/RGB repair: `b83157e60311740be9db1e0797642511876b3e39`.
- Reset progress is preserved across RST/EN reboots so the multi-step physical sequence can be recognized.
- RGB gives visible acknowledgement of accepted reset steps; the final erase/reset path has its own confirmation indication.
- CI contains a guard so the obsolete GPIO21 reset logic is not accidentally restored.
- Hardware validation on 2026-08-21 disproved the original RTC-retention assumption; see the confirmed diagnosis/fix below.

### Web UI repair
- Working branch: `fix/setup-ui-clean-20260821`.
- PR: #72, `Stabilize first-boot setup layout and enlarge status markers`.
- Base is the RST/RGB-fixed mainline commit `b83157e60311740be9db1e0797642511876b3e39`, so this branch contains both areas of work.
- Removed the historical stack of conflicting 52/57/58/59vw setup overrides.
- Replaced it with one deterministic responsive strategy.
- Desktop first-boot setup: compact/top-left bounded card, Bruce bounded on the right, readable controls, bounded two-column Wi-Fi results.
- Tablet/mobile collapse is explicit and deterministic.
- Dashboard/status dots were enlarged for readability.
- Setup UI contract was extended to prevent the obsolete stacked CSS overrides from returning.

### Browser navigation failure — cause and resolution
- Earlier run `32444212928`, job `96660653512` (`Browser navigation invariant`) was red.
- Important: this was not evidence of a functional hash-route bug. The failure was a CI/runtime-test timeout.
- Exact symptom: old `tools/check_web_navigation_runtime.py` launched Chrome repeatedly for cold-load routes; the first `#overview` Chrome invocation timed out after 25 seconds.
- Static `Web UI contract` had already passed in that run.
- First repair simplified the runtime audit to one Chromium document/session probe instead of spawning a fresh browser process for every route.
- A later head (`bae564f`) exposed a second runner-dependent failure mode: even the single-document `--dump-dom` process could remain alive after the JavaScript probe had finished. Run `32450230077` failed twice on two different runners with the same 20-second `--dump-dom` timeout while the static Web UI contract passed both times.
- Final repair commit: `3de92710b3c97c62ca709abe7c4436cc6eee143c` (`Make browser navigation audit callback-driven`).
- The browser probe now posts PASS/FAIL back to the local Python test server; Python terminates Chromium explicitly after receiving the callback. This removes dependence on `--dump-dom` process exit while preserving real click/hash transitions, settings-page visibility, focus safety, unknown hashes, burst routing and same-hash clicks.
- Do not return to multi-Chrome-per-route or dump-dom-exit-driven navigation testing unless there is demonstrated evidence that the callback probe is insufficient.

### Verified green CI for Build #1813 baseline
All of the following completed successfully for `3de92710b3c97c62ca709abe7c4436cc6eee143c`:
- Web Navigation Runtime Audit — run `32506986002`, run #48 — SUCCESS.
- Web UI Preview — run `32506986079`, run #439 — SUCCESS.
- HomeGuard Wi-Fi Stability — run `32506986072`, run #16 — SUCCESS.
- Web Navigation Audit — run `32506986165`, run #33 — SUCCESS.
- Setup UI Contract — run `32506986128`, run #44 — SUCCESS.
- HomeGuard-S3 Build — run `32506986156`, build/run #1813 — SUCCESS.

Build #1813 job results:
- ESP-IDF 5.4.4 firmware — SUCCESS; firmware build, checksum generation and upload all passed.
- Host validation — SUCCESS; project preflight, ESP-IDF/source/dependency/GPIO/security/factory-reset/partition/unit/Web/LAN/Android/cloud/browser/mock checks all passed.
- Android debug APK — SUCCESS; policy tests, debug APK build, installer verification and artifact upload passed.

### Build #1813 artifacts used for hardware test
From workflow run `32506986156`, code head `3de92710b3c97c62ca709abe7c4436cc6eee143c`:

- Firmware artifact: `HomeGuard-S3-firmware`
  - Artifact ID: `9455702180`
  - ZIP size reported by GitHub: 9,934,270 bytes
  - Digest: `sha256:3add738422e74a8806837c0ff5a0962fd869127f9bb88fcce604bbd6dbd909c8`
- ESP-IDF diagnostics: `HomeGuard-S3-ESP-IDF-diagnostics`
  - Artifact ID: `9455700967`
  - Digest: `sha256:7899b55347d03e08d241516d1fdf62ba87f34b4c45a58628a4c2b66c2bc3161b`
- Android artifact: `MyFist-Android`
  - Artifact ID: `9455635792`
  - Digest: `sha256:0106f20b3832e1cdeaf1749acc0d8e04630ce149a7094e16ff10e0b757c20434`
- Host diagnostics: `HomeGuard-S3-host-diagnostics`
  - Artifact ID: `9455625576`
  - Digest: `sha256:77df9c9b4856645a5c44217673735fb52b32af87278a0020914106bcc7490c18`

### Hardware test — Build #1813, 2026-08-21 evening
- Firmware artifact `9455702180` was extracted to `C:\HomeGuard-S3-firmware (7)`.
- Flash was fully erased on COM6 using esptool 5.3.1; erase completed successfully.
- Full image was then written successfully at the established offsets:
  - `0x0` bootloader;
  - `0x8000` partition table;
  - `0xF000` OTA data;
  - `0x20000` `homeguard_s3.bin`.
- Every flashed image reported `Hash of data verified`.
- After boot, the controller successfully exposed provisioning AP `HomeGuard-S3-A711`; the Windows PC connected to it. This proves the firmware boots and the Wi-Fi provisioning path is alive.
- Normal idle RGB after clean boot was OFF.
- First physical RST/EN test: one short press was performed. Expected contract was a WHITE acknowledgement for 1500 ms. Actual result: **RGB did not light at all**.
- Serial log was then captured on COM6 at 115200 and the physical RST was pressed once again for diagnosis.

### Confirmed RST root cause from COM6 log
- Initial boot after flashing reported ROM reset reason `rst:0x1 (POWERON)` and application log `Reset reason=1 is not a physical RST gesture step`.
- After the user physically pressed RST/EN, ROM again reported exactly `rst:0x1 (POWERON)`.
- On that post-button boot the application again logged `Reset reason=1 is not a physical RST gesture step`.
- Therefore the WHITE command was never reached. The RGB driver was not the cause of this particular failure.
- This proves the previous assumption was wrong on the real HW-678 path: `RTC_NOINIT` did not retain the marker across the board's EN reset in the way the detector required.
- Important: on this hardware the firmware-visible reset reason does not distinguish a true power-on from the physical EN/RST button. Software cannot reconstruct that distinction after reset without an additional hardware signal.

### RST detector repair after hardware evidence
- Removed dependence on `RTC_NOINIT_ATTR` / `g_rst_boot_marker` from the physical RST detector.
- Added a persistent NVS baseline marker in namespace `hg_rstseq`, key `boot_seen`.
- First boot with fresh NVS establishes the baseline marker and is deliberately **not counted** as a reset gesture.
- After that baseline exists, a `POWERON` reset can advance the physical RST sequence; `ESP_RST_EXT` remains accepted as well.
- Accepted step behavior is unchanged: WHITE acknowledgement for 1500 ms, progress stored in NVS, abandoned progress cleared after the 5-second inter-step window.
- Third accepted step remains: WHITE -> delay -> OFF -> stage Factory Reset -> reboot; successful erase remains RED for 5 seconds -> OFF -> reboot.
- Unit test was updated to represent the persistent baseline instead of the disproved RTC-retention assumption.
- CI contract now explicitly rejects regression to `RTC_NOINIT_ATTR`, `g_rst_boot_marker`, or `rtc_state_was_valid` and requires the NVS `boot_seen` path.
- Relevant repair commits in sequence:
  - `0a3269b` — reset helper semantics changed from RTC marker to persistent boot marker;
  - `fa5d232` — runtime switched to NVS `boot_seen` baseline;
  - `3430f43` — unit tests updated;
  - `180c6d6` — CI reset/RGB contract updated to lock the hardware-proven model.

### Safety limitation of the hardware-proven model
- Because both real power-up and EN/RST present as `POWERON`, firmware alone cannot tell them apart on this board.
- Consequently, after the baseline exists, **three rapid power cycles within the same inter-step timing rules are electrically indistinguishable from three rapid RST presses and can invoke Factory Reset**.
- A normal single power cycle can at most become step 1 and is automatically cleared after the timeout; destructive reset still requires the full three-step gesture.
- Eliminating this ambiguity completely would require a separate hardware-observable button signal or another retained/time source independent of the EN reset.

### Immediate next step
1. Wait for CI on the new RST fix head to become fully green.
2. Use the newly generated firmware artifact, not Build #1813.
3. Flash the ESP32-S3 on COM6 using the established four-image layout.
4. Keep serial log open at 115200 and press physical RST once.
5. Expected result: log `Physical RST accepted: WHITE acknowledgement, step 1/3` and visible WHITE for 1500 ms.
6. Only after step 1 is physically proven, test step 2 and then the complete 3-step Factory Reset sequence.
7. Continue PC Web UI testing after RST/RGB is physically proven.

### Hardware communication decision — valve nodes
- For the planned distributed motorized water-valve control, do not buy/use the MCP2515 + TJA1050 SPI CAN module merely for this task; it is unnecessary complexity for the current architecture.
- The current preferred direction for the small valve nodes is LIN-style communication: small local driver/node per valve and a master-side interface at ESP32-S3, subject to final electrical design when the actual valve hardware is opened/inspected.
- If CAN is reconsidered later, ESP32-S3 already provides a TWAI/CAN controller, so an external MCP2515 is generally unnecessary; a suitable 3.3 V CAN transceiver is the cleaner direction.

## Maintenance rule
Append new dated checkpoints below/above this section after each significant work session. Never delete historical failure causes merely because the current build is green; those notes prevent repeating old mistakes.
