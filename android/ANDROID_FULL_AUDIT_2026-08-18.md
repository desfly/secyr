# HomeGuard-S3 Android full audit — 2026-08-18

Branch: `audit/android-full-20260818`  
Base: `main` @ `e522dc7ffc1a716f370ac7050b58c6f1a23e618a`  
Draft PR: `#53`

## Goal
One canonical runtime path for discovery, registry, selection, auth/session, provisioning, telemetry/reconnect, Factory Reset and backup/restore. Correctness and lifecycle stability first; simplification and measurable resource reduction second.

## Findings register

### CRITICAL
- **A-001 — Plaintext first-run password — FIXED.** First-run password moved to Android Keystore-backed `SecureTokenStore`, with legacy plaintext migration/removal.

### HIGH
- **A-002 — Discovery dedup was host-based — FIXED.** Physical-controller identity is reconciled transitively through `ControllerIdentity.sameController()`.
- **A-003 — Oversized/duplicated Bruce — FIXED.** Normal Device List uses only the compact Bruce mark.
- **A-007 — Stale WebSocket frames after device switch — FIXED.** Frames/callbacks from replaced sockets are ignored.
- **A-008 — Factory Reset transport errors misclassified as success — FIXED.** Disconnect counts as expected destructive-reset loss only after request-body submission.
- **A-009 — Device secret reuse/restore leakage — FIXED.** API/telemetry secrets are device-bound; controller changes clear carried-over secrets.
- **A-010 — Add Device was a prominent primary action — FIXED.** Add remains secondary to the device list.
- **A-011 — Manual search progress represented UDP only — FIXED.** Coordinator combines UDP + HTTP progress until both complete.
- **A-014 — HTTP discovery retained stale devices after Wi-Fi loss — FIXED.** Missing Wi-Fi/IPv4 clears HTTP results.
- **A-015 — Duplicate discovery counters — FIXED.** Search state/result count is rendered once.
- **A-016 — Provisioning stable-ID comparison case-sensitive — FIXED.** Stable IDs and HTTPS handling normalized case-insensitively.
- **A-017 — QR provisioning bypassed owner-friendly-name contract — FIXED.** QR path requires owner name and registers stable controller ID before endpoint refresh.
- **A-018 — ProvisioningScreen owned a second discovery/settings runtime — FIXED.** Removed screen-owned coordinator/store, duplicate start/stop and `Activity.recreate()` workaround.
- **A-019 — HttpDeviceApi cancellation could leak Response — FIXED.** Cancellation closes/cancels undeliverable OkHttp work.
- **A-022 — Stale discovery could remain visible without a new source emission — FIXED + CI VALIDATED.** One 30-second freshness rule plus 1-second expiry tick. Build #1153 passed.
- **A-023 — Provisioning shortcuts bypassed registry naming — FIXED.** Already-connected/manual-IP paths require friendly name and canonical registry/settings path.
- **A-024 — Setup AP cancellation could leave process bound to temporary Wi-Fi — FIXED.** Terminal/cancel cleanup unbinds/unregisters safely.
- **A-025 — Rapid repeated manual scan could launch overlapping full scans — FIXED.** `rescan()` is single-flight.
- **A-026 — Telemetry stayed offline forever after transient WebSocket failure — FIXED.** One reconnect job with 2/5/10/20/30-second capped backoff; target changes/stop/CONNECTED/UNAUTHORIZED cancel retries.
- **A-027 — Synchronous telemetry connect setup failure could kill DeviceSession collector — FIXED + CI VALIDATED.** Malformed URL/pin setup failures become `OFFLINE`; collector survives. Build #1146 passed.
- **A-028 — Invalid manual IP closed provisioning — FIXED + CI VALIDATED.** Screen closes only after successful validation + registry/settings update. Build #1153 passed.
- **A-029 — NSD start/restart races could poison discovery lifecycle — FIXED + CI VALIDATED.** Start rollback + volatile callback state + per-run generation token prevent old resolves contaminating new sessions. Build #1155 passed.
- **A-030 — Discovery refresh caused repeated persistent registry writes — FIXED, CI PENDING ON NEWEST HEAD.** UDP runs every 5 s; timestamp-only registry persistence is now throttled to 60 s while real ID/base-URL changes persist immediately. Theoretical steady-state timestamp writes drop from up to 12/minute to 1/minute per controller (~12× reduction). Three unit tests cover boundary/endpoint/case behavior.
- **A-031 — Duplicate telemetry sequence could still trigger duplicate history write/notification — FIXED, CI PENDING ON NEWEST HEAD.** Event list already deduplicated visually, but duplicate `sequence` still reached `liveEvents`. Duplicate sequence is now suppressed before history/notification fan-out; focused test added.
- **A-032 — No-op settings/registry mutations still wrote SharedPreferences/secure storage and emitted Flow state — FIXED, CI PENDING ON NEWEST HEAD.** Stores now return early when normalized state/list is unchanged, reducing disk I/O and recomposition/reconnect triggers without changing real updates.
- **A-033 — HTTP subnet discovery accepted unbounded 200-OK response bodies — FIXED, CI PENDING ON NEWEST HEAD.** Discovery JSON read is capped at 64 KiB characters; oversized LAN responses are rejected instead of growing heap arbitrarily. Boundary test added.
- **A-034 — DeviceEndpointResolver duplicated registry/settings reconciliation side effects — FIXED, CI PENDING ON NEWEST HEAD.** Resolver is now read-only. Discovery reconciliation remains in one MainActivity-owned collector, eliminating duplicate coroutine writes/feedback paths for the selected controller.
- **A-035 — Event history reparsed all persisted JSON on every new event — FIXED, CI PENDING ON NEWEST HEAD.** History is loaded once into a synchronized in-memory cache; each genuinely new event still persists, but append no longer reparses up to 256 prior records from disk. Duplicate sequence is a zero-write no-op. Unit test covers duplicate/no-op and bounded ordering.
- **A-036 — Parallel HTTP scan progress could regress numerically due to out-of-order worker updates — FIXED, CI PENDING ON NEWEST HEAD.** Only the short progress record/update is serialized; up to 32 HTTP probes remain parallel.

### MEDIUM
- **A-004 — MainActivity is still an orchestration god-object — OPEN.** Do not decompose mechanically until lifecycle contracts are frozen; avoid adding parallel coordinators/view-model layers.
- **A-005 — Global cleartext HTTP enabled — OPEN, CURRENTLY REQUIRED BY CONTRACT.** `usesCleartextTraffic=true` is broad, but current setup/manual/fallback discovery intentionally talks HTTP to arbitrary LAN IPv4 addresses. Android domain allowlists cannot safely express arbitrary RFC1918/manual IP targets. Disabling globally now would break proven local/setup behavior; revisit only with a protocol/endpoint design change.
- **A-012 — Selected controller comparison case-sensitive — FIXED.**
- **A-013 — Legacy unnamed devices synthesized `HomeGuard` — FIXED.** UI shows `Потрібна назва` until owner rename.
- **A-020 — Device-state picons mixed authorization/armed/UNKNOWN — FIXED.** Authorization, unknown telemetry and alarm/fault states are distinct.

### CLEANUP
- **A-006 — Parallel/dead Android architecture — FIXED + CI VALIDATED.** Removed the complete dead island atomically: five `ua.homeguard.app.alarm...` source files, `HomeGuardApi`, `DeviceDtos`, `HomeGuardRepository`, `MainUiState`, `MainViewModel`, and old `AlarmAcknowledgementMappingTest`. Total **11 files**, **293 production source lines + 41 test lines = 334 lines removed**. Build #1143 passed on exact cleanup head.
- **A-021 — RegisteredDeviceStore process-global `activeStore` bridge — FIXED.** Removed static registry mutation hooks; app-owned store is passed explicitly to consumers.

## Quantitative audit metrics
Measured only where the repository/CI provides real numbers. Phone-only metrics are intentionally not invented.

### Code / structure
- Base: `e522dc7ffc1a716f370ac7050b58c6f1a23e618a`.
- Latest code/test head before this register update: `19590ba75592e7db82c94b08c273d7bf339679f0`.
- Commits ahead of base: **93**.
- PR footprint at that head: **46 changed files, 1,390 additions, 707 deletions** (production + tests + audit docs; not a pure LOC reduction metric).
- Proven-dead island: **11 files / 334 lines removed**, including **293 production lines**.
- `DeviceEndpointResolver`: current PR diff versus base is **1 addition / 25 deletions** after making it read-only.

### APK / CI artifact
- Build #1155 `MyFist-Android` artifact ZIP: **9,611,209 bytes**.
- Extracted `MyFist.apk`: **10,018,339 bytes**.
- A matching exact-base Android artifact was not found for base commit, therefore **no fabricated before/after APK percentage is reported**.

### Runtime-contract numbers visible in code/tests
- Background UDP discovery cadence: **5 s**.
- Discovery freshness window: **30 s**; expiry reevaluation tick: **1 s**.
- Registry last-seen persistence throttle: **60 s**; steady timestamp-only upper bound reduced from **12/min → 1/min per continuously discovered controller**.
- HTTP subnet scan parallelism: **32 probes**.
- HTTP discovery response cap: **64 KiB characters per JSON response**.
- Telemetry reconnect backoff: **2 / 5 / 10 / 20 / 30 s**, capped at 30 s.
- Event history cap: **256 records**; append now performs **0 persisted-history reads per new event after store construction** versus 1 full persisted-history read/parse per append previously.

## Phone benchmark plan after code freeze
Use the same phone, same Android version, same Wi-Fi/AP, same ESP firmware/config, same charger state, and at least 5 runs per measurement. Record median and p95 where practical.

1. **Cold start:** force-stop app, clear from recents, launch via `am start -W`; record `ThisTime/TotalTime/WaitTime` for 5-10 runs.
2. **Warm start:** return to launcher/another app without force-stop, relaunch; same timing capture.
3. **Memory:** after 60 s idle, during active discovery, and during live telemetry capture `dumpsys meminfo <package>`: Total PSS/RSS, Java Heap, Native Heap, Graphics/Other.
4. **CPU:** sample `top`/`dumpsys cpuinfo` for 60 s in idle, discovery and telemetry states; compare average and peak.
5. **Discovery latency:** from manual scan tap to first matching ESP card and to scan-complete; repeat 10 times.
6. **Reconnect:** disable/re-enable Wi-Fi and separately reboot/switch ESP; measure OFFLINE → CONNECTED and verify the expected 2/5/10/20/30 s retry policy rather than overlapping retries.
7. **Lifecycle soak:** 50 foreground/background cycles, 20 device switches, 20 repeated scan taps, Wi-Fi loss/recovery loops; verify one card/controller, no duplicate notifications, no leaked discovery jobs and no stale telemetry.

## Current execution status
- Build #1169 on code/test head `d66b4302...`: **Host validation passed; production Android Kotlin compiled; Android job failed only in `compileDebugUnitTestKotlin` because the newly added `EventHistoryMergeTest` imported unavailable `kotlin.test` symbols.**
- Test-only fix commit `19590ba75592e7db82c94b08c273d7bf339679f0` switches that test to the project-standard JUnit4 imports; no production behavior changed.
- Exact-head Build #1171 for `19590ba7...` is queued and must pass before A-030..A-036 are marked CI validated.
- `main` remains untouched; PR #53 remains draft and unmerged.

## Next immediate blocks
1. Finish exact-head #1171; if red, fix the exact Android/host failure first.
2. One final MainActivity/lifecycle pass for permission/start-stop state and any remaining duplicate collector/write path.
3. Freeze Android code only after green exact-head CI.
4. Produce final CI-measured before/after report; then perform the phone benchmark plan once, not as intermediate testing.

## Audit rule
Do not merge to `main` during discovery. Keep all work isolated on `audit/android-full-20260818` / draft PR #53 until cleanup/lifecycle/reconnect is complete, exact-head CI is green, and later phone validation passes.
