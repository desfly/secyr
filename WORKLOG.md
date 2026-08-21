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
- Status: implemented in code; next required validation is physical testing on the ESP32-S3 after flashing the combined firmware.

### Web UI repair
- Working branch: `fix/setup-ui-clean-20260821`.
- PR: #72, `Stabilize first-boot setup layout and enlarge status markers`.
- Current verified head: `49770d80c2a73a9580156e673e180240ebb980dd`.
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
- Resolution: navigation runtime audit was simplified to one Chromium document/session probe instead of spawning a fresh browser process for every route. It still checks real click/hash transitions, settings-page visibility, focus safety, unknown hashes, burst routing and same-hash clicks.
- Do not return to the old multi-Chrome-per-route audit unless there is a demonstrated need.

### Verified green CI for head 49770d8
All of the following completed successfully for `49770d80c2a73a9580156e673e180240ebb980dd`:
- Setup UI Contract — run `32445179310`, run #42 — SUCCESS.
- Web UI Preview — run `32445179165`, run #437 — SUCCESS.
- Web Navigation Audit — run `32445179258`, run #31 — SUCCESS.
- Web Navigation Runtime Audit — run `32445179244`, run #46 — SUCCESS.
- HomeGuard Wi-Fi Stability — run `32445179303`, run #14 — SUCCESS.
- HomeGuard-S3 Build — run `32445179174`, build/run #1811 — SUCCESS.

### Firmware artifact ready for hardware test
- Workflow run: `32445179174` / HomeGuard-S3 Build #1811.
- Artifact: `HomeGuard-S3-firmware`.
- Artifact ID: `9433858527`.
- Artifact size: 9,934,277 bytes (ZIP archive size reported by GitHub Actions).
- Artifact digest: `sha256:cd465247dcf1d236f968930914425a2e2e49d4e142fcaba60f2a3a5250dd36a4`.
- Head branch: `fix/setup-ui-clean-20260821`.
- Head SHA: `49770d80c2a73a9580156e673e180240ebb980dd`.
- This is the current combined candidate containing the web UI work on top of the RST/RGB-fixed base.

### What is and is not proven
PROVEN BY CI:
- firmware builds successfully;
- static Web UI contract passes;
- browser navigation audit passes;
- browser runtime navigation audit passes;
- Web UI preview workflow passes;
- Wi-Fi stability checks pass.

NOT YET PROVEN ON HARDWARE:
- physical RST/EN multi-step reset behavior on the actual ESP32-S3;
- RGB indications as seen on the actual board;
- complete PC Web UI behavior against the newly flashed device;
- mobile UI/device test remains after desktop validation.

### Immediate next step
1. Use the `HomeGuard-S3-firmware` artifact from Build #1811 / artifact `9433858527`.
2. Flash the ESP32-S3 using the established COM6 procedure and the artifact's flash layout/files.
3. Test RST/RGB physically first and record exact observed sequence.
4. Continue the PC Web UI test from the last unfinished point; do not restart already-passed checks without reason.
5. After desktop Web UI passes, proceed to mobile UI testing.
6. Record every hardware result here with build/run/SHA before making another code change.

### Hardware communication decision — valve nodes
- For the planned distributed motorized water-valve control, do not buy/use the MCP2515 + TJA1050 SPI CAN module merely for this task; it is unnecessary complexity for the current architecture.
- The current preferred direction for the small valve nodes is LIN-style communication: small local driver/node per valve and a master-side interface at ESP32-S3, subject to final electrical design when the actual valve hardware is opened/inspected.
- If CAN is reconsidered later, ESP32-S3 already provides a TWAI/CAN controller, so an external MCP2515 is generally unnecessary; a suitable 3.3 V CAN transceiver is the cleaner direction.

## Maintenance rule
Append new dated checkpoints below/above this section after each significant work session. Never delete historical failure causes merely because the current build is green; those notes prevent repeating old mistakes.
