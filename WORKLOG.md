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
- Mainline checkpoint containing the RST/RGB repair: `b83157e60311740be9db1e0797642511876b3e39`.
- Reset progress is preserved across RST/EN reboots so the multi-step physical sequence can be recognized.
- RGB gives visible acknowledgement of accepted reset steps; the final erase/reset path has its own confirmation indication.
- CI contains a guard so the obsolete GPIO21 reset logic is not accidentally restored.
- Status: implemented in code; hardware validation is now in progress.

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

### Verified green CI for current code head 3de9271
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

### Current artifacts ready for hardware/device test
From HomeGuard-S3 Build #1813 / workflow run `32506986156`, code head `3de92710b3c97c62ca709abe7c4436cc6eee143c`:

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

The older Build #1811 artifact remains historical evidence only. For hardware validation, use Build #1813.

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
- Therefore physical RST/RGB hardware validation is **FAILED at step 1** despite green CI.
- Do not proceed to a second/third RST step until the first-step failure is diagnosed; otherwise state may be ambiguous.

### Current RST diagnosis from source
- `hg_reset_sequence.cpp` only calls WHITE after `reset_press_detected(...)` accepts the boot as a physical reset.
- The current detector accepts `ESP_RST_EXT`, or `ESP_RST_POWERON` only when an `RTC_NOINIT` marker survived the reset.
- The source explicitly assumes this board's RST/EN button reports `POWERON` and that the RTC marker survives that reset.
- Current hardware evidence is compatible with either: (a) that retention/reset-reason assumption is wrong on this board/reset path, or (b) WHITE was requested but the RGB write failed. These must be distinguished by boot log or an independent RGB diagnostic before changing product logic.
- RGB driver is WS2812 via RMT on board GPIO48; previous project testing had already established that the onboard RGB can operate, so do not change the RGB pin blindly.

### Immediate next step
1. Capture the boot log across one physical RST press and read the `hg_rst_sequence` reset-reason / acknowledgement messages.
2. If the log shows the reset was not accepted, repair the physical-reset detector based on observed reset reason/retention behavior.
3. If the log shows `Physical RST accepted` but no light, isolate the RGB/RMT path instead.
4. Rebuild, obtain a new green firmware artifact, flash it, and repeat step 1 only.
5. Continue PC Web UI testing only after the RST/RGB contract is physically proven.

### Hardware communication decision — valve nodes
- For the planned distributed motorized water-valve control, do not buy/use the MCP2515 + TJA1050 SPI CAN module merely for this task; it is unnecessary complexity for the current architecture.
- The current preferred direction for the small valve nodes is LIN-style communication: small local driver/node per valve and a master-side interface at ESP32-S3, subject to final electrical design when the actual valve hardware is opened/inspected.
- If CAN is reconsidered later, ESP32-S3 already provides a TWAI/CAN controller, so an external MCP2515 is generally unnecessary; a suitable 3.3 V CAN transceiver is the cleaner direction.

## Maintenance rule
Append new dated checkpoints below/above this section after each significant work session. Never delete historical failure causes merely because the current build is green; those notes prevent repeating old mistakes.
