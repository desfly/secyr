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
- Historical Build #1821 contract used three accepted steps for full Factory Reset; this was later superseded by the 3-step settings / 5-step factory contract recorded below.
- Unit test was updated to represent the persistent baseline instead of the disproved RTC-retention assumption.
- CI contract explicitly rejects regression to `RTC_NOINIT_ATTR`, `g_rst_boot_marker`, or `rtc_state_was_valid` and requires the NVS `boot_seen` path.
- Relevant repair commits in sequence:
  - `0a3269b` — reset helper semantics changed from RTC marker to persistent boot marker;
  - `fa5d232` — runtime switched to NVS `boot_seen` baseline;
  - `3430f43` — unit tests updated;
  - `180c6d6` — CI reset/RGB contract updated to lock the hardware-proven model.

### Safety limitation of the hardware-proven model
- Because both real power-up and EN/RST present as `POWERON`, firmware alone cannot tell them apart on this board.
- Consequently, rapid power cycles remain electrically indistinguishable from rapid RST presses under the same sequence rules.
- Eliminating this ambiguity completely would require a separate hardware-observable button signal or another retained/time source independent of the EN reset.

### Build #1821 hardware validation
- Build #1821 successfully flashed on COM6 without errors; `homeguard_s3.bin` size was 1,803,264 bytes and every image reported `Hash of data verified`.
- Physical RST/EN sequence was observed as **three WHITE acknowledgements followed by one RED confirmation** under the then-current 3-step full-factory contract.
- Post-reset serial log showed `Reset reason=3 is not a physical RST gesture step`, proving the software reboot after destructive reset was not miscounted.
- The post-reset log also showed no persisted access database, no commissioning state, and provisioning AP startup on `192.168.4.1`, confirming the user-owned reset state was actually erased.

### Superseding physical RST contract: 3-step settings / 5-step factory
- The RST/EN contract was extended after hardware validation so one physical button can expose two reset levels without impossible hold-time detection on EN.
- **3 rapid accepted RST steps + no continuation for the 5-second inter-step window = Settings Reset.**
- Settings Reset erases Wi-Fi, cloud, controller config, provisioning and commissioning progress but preserves `hg_access` users and immutable factory/hardware identity.
- **5 rapid accepted RST steps = full user Factory Reset.**
- Full Factory Reset erases the same settings state plus `hg_access` users. Immutable factory identity and hardware verification remain preserved by design.
- Step 3 does not erase immediately; it arms Settings Reset and waits. Step 4 cancels the settings action and extends toward full factory. If step 5 does not arrive, step 4 times out with no reset.
- Per-step acknowledgement remains WHITE for about 1.5 seconds after release of EN. Successful Settings Reset confirms WHITE for 5 seconds. Successful full Factory Reset confirms RED for 5 seconds.
- The reason RGB only appears after button release is physical: while EN is held low the ESP32-S3 is in reset and cannot execute RGB code.
- Build #1832 compiled this 3/5 contract successfully and was flashed without `erase-flash` so access state could be preserved for the settings-reset test.

### Build #1832 post-flash WHITE observation and root cause
- After flashing Build #1832 with esptool, the user observed a short WHITE indication immediately after the automatic `Hard resetting via RTS pin...` reboot.
- This exposed another consequence of HW-678 reporting EN-like resets as `POWERON`: once `boot_seen` existed, the automatic post-flash RTS/EN reset could be counted as RST step 1 even though the user had not pressed the physical button.
- This behavior is undesirable because it preloads the RST sequence and makes firmware updates look like user gestures.

### Firmware-baseline suppression fix
- Added NVS firmware baseline signature `hg_rstseq/fw_sig` derived from the already compiled `HG_GIT_REVISION`.
- On the first boot after the firmware revision changes, runtime refreshes `fw_sig`, clears any abandoned RST sequence, establishes `boot_seen` if needed, logs `Firmware RST baseline refreshed; reset reason=... is not counted`, and returns **before any physical-RST WHITE acknowledgement**.
- This intentionally suppresses the automatic first post-flash/OTA reset for a new firmware revision.
- It does **not** claim to distinguish a physical EN press from an ordinary power-cycle when the same firmware is already running; hardware still reports both as `POWERON`.
- Unit tests cover stable/different firmware signatures and the baseline-change predicate.
- `check_reset_rgb_contract.py` now requires `fw_sig`, firmware-baseline refresh, sequence clear, and ordering before physical WHITE so this regression cannot silently return.
- Runtime uses an NVS blob for the 32-bit signature and a safe `HG_GIT_REVISION="unknown"` fallback for host mock compilation; real CI/ESP-IDF builds still receive the actual Git revision from CMake.
- Repair commits include `7784173`, `7d2021f`, `04feeea`, `cc40d1e`, and mock-compatibility fix `88858d3a29ed2e4feba9da60a01a8ad16d58d5c6`.

### Verified green CI for post-flash suppression — Build #1837
For code head `88858d3a29ed2e4feba9da60a01a8ad16d58d5c6`:
- Setup UI Contract #68 — SUCCESS.
- HomeGuard Wi-Fi Stability #40 — SUCCESS.
- Web Navigation Runtime Audit #72 — SUCCESS.
- Web UI Preview #463 — SUCCESS.
- Web Navigation Audit #57 — SUCCESS.
- HomeGuard-S3 Build run `32514377122`, build #1837 — SUCCESS.
- Build #1837 Host validation — SUCCESS, including source audit, access boundary, factory reset coverage, unit tests, browser smoke, mock syntax and mock link.
- Build #1837 Android debug APK — SUCCESS.
- Build #1837 ESP-IDF 5.4.4 firmware — SUCCESS; firmware build and checksum generation passed.
- Firmware artifact: `HomeGuard-S3-firmware`, artifact ID `9458305693`, digest `sha256:6f38a3133679c7808e30acf27ebf2169ab243229062c89cae9a9b72b804a7bd1`.

### Immediate hardware test after Build #1837
1. Use Build #1837 artifact `9458305693`; flash **without `erase-flash`** so the access database can survive the settings-reset test.
2. Immediately after esptool's `Hard resetting via RTS pin...`, expected RGB is **no short WHITE**. Optional serial confirmation: `Firmware RST baseline refreshed; reset reason=... is not counted`.
3. Ensure an Admin exists before the reset-level test.
4. Perform exactly 3 rapid physical RST presses, then stop. Expected: three per-step WHITE acknowledgements, wait out the window, Settings Reset executes, WHITE 5-second success; the Admin must still exist while Wi-Fi/settings are cleared.
5. Reconfigure mutable state as needed, then perform 5 rapid RST presses. Expected: five per-step WHITE acknowledgements, full Factory Reset, RED 5-second success, and the first-Admin bootstrap screen must return.
6. Record actual hardware results before declaring the new 3/5 contract fully hardware-validated.

### Build #1837 hardware validation — post-flash baseline and 3-step Settings Reset
- Build #1837 (`88858d3a29ed2e4feba9da60a01a8ad16d58d5c6`, artifact `9458305693`) was flashed on COM6 without `erase-flash`; `homeguard_s3.bin` size was 1,805,408 bytes and all flashed images reported `Hash of data verified`.
- Immediately after esptool `Hard resetting via RTS pin...`, the onboard RGB **did not light WHITE**. This is the expected hardware result and confirms the `fw_sig` post-flash baseline suppression works on the real HW-678 board.
- The user then performed exactly 3 rapid physical RST/EN presses under the new 3/5 contract.
- Actual result: Wi-Fi credentials/settings were erased and the controller returned to provisioning/network setup state.
- Crucially, the existing Admin was preserved: the Web UI showed **login for the existing Admin**, not the first-Admin creation/bootstrap screen.
- Therefore the 3-step Settings Reset is **hardware PASS**: mutable settings are erased while `hg_access` users remain intact.
- Remaining reset validation: perform the 5-step full Factory Reset and verify five WHITE acknowledgements, RED 5-second success confirmation, and return to the first-Admin creation screen.

### Hardware communication decision — valve nodes
- For the planned distributed motorized water-valve control, do not buy/use the MCP2515 + TJA1050 SPI CAN module merely for this task; it is unnecessary complexity for the current architecture.
- The current preferred direction for the small valve nodes is LIN-style communication: small local driver/node per valve and a master-side interface at ESP32-S3, subject to final electrical design when the actual valve hardware is opened/inspected.
- If CAN is reconsidered later, ESP32-S3 already provides a TWAI/CAN controller, so an external MCP2515 is generally unnecessary; a suitable 3.3 V CAN transceiver is the cleaner direction.

## Maintenance rule
Append new dated checkpoints below/above this section after each significant work session. Never delete historical failure causes merely because the current build is green; those notes prevent repeating old mistakes.