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
- **A-030 — Discovery refresh caused repeated persistent registry writes — FIXED + CI VALIDATED.** UDP runs every 5 s; timestamp-only registry persistence is throttled to 60 s while real ID/base-URL changes persist immediately. Steady-state timestamp writes drop from up to 12/minute to 1/minute per controller (~12×). Tests cover boundary/endpoint/case behavior. Build #1172 passed on a descendant of the code/test head.
- **A-031 — Duplicate telemetry sequence could still trigger duplicate history write/notification — FIXED + CI VALIDATED.** Duplicate `sequence` is suppressed before history/notification fan-out; focused test added. Build #1172 passed.
- **A-032 — No-op settings/registry mutations still wrote SharedPreferences/secure storage and emitted Flow state — FIXED + CI VALIDATED.** Stores return early when normalized state/list is unchanged. Build #1172 passed.
- **A-033 — HTTP subnet discovery accepted unbounded 200-OK response bodies — FIXED + CI VALIDATED.** Discovery JSON is capped at 64 KiB characters. Build #1172 passed.
- **A-034 — DeviceEndpointResolver duplicated registry/settings reconciliation side effects — FIXED + CI VALIDATED.** Resolver is read-only; discovery reconciliation remains in one MainActivity-owned collector. Build #1172 passed.
- **A-035 — Event history reparsed all persisted JSON on every new event — FIXED + CI VALIDATED.** History is loaded once into a synchronized memory cache; duplicate sequence is a zero-write no-op. Build #1172 passed.
- **A-036 — Parallel HTTP scan progress could regress numerically due to out-of-order worker updates — FIXED + CI VALIDATED.** Only the short progress state update is serialized; 32 HTTP probes remain parallel. Build #1172 passed.
- **A-037 — Same physical device with different ID case could clear route/certificate or survive deletion as selected — FIXED, CI PENDING ON NEWEST HEAD.** `DeviceSelectionPolicy` now treats IDs case-insensitively when deciding whether a controller really changed, preserving remembered local URL/certificate for case-only changes. Device-list deletion also clears active selection case-insensitively. Unit test covers `HG-AbC123` → `hg-abc123`.
- **A-038 — Wi-Fi scan HTTP body cap was missing when Content-Length was present — FIXED, CI PENDING ON NEWEST HEAD.** Both Content-Length and connection-close response paths are now capped at 256,000 bytes, preventing an oversized LAN response from growing heap arbitrarily during provisioning Wi-Fi scan.

### MEDIUM
- **A-004 — MainActivity is still an orchestration god-object — OPEN.** Do not decompose mechanically until lifecycle contracts are frozen; avoid adding parallel coordinators/view-model layers.
- **A-005 — Global cleartext HTTP enabled — OPEN, CURRENTLY REQUIRED BY CONTRACT.** `usesCleartextTraffic=true` is broad, but setup/manual/fallback discovery intentionally talks HTTP to arbitrary LAN IPv4 addresses. Disabling globally now would break local/setup behavior; revisit only with a protocol/endpoint design change.
- **A-012 — Selected controller comparison case-sensitive — FIXED.**
- **A-013 — Legacy unnamed devices synthesized `HomeGuard` — FIXED.** UI shows `Потрібна назва` until owner rename.
- **A-020 — Device-state picons mixed authorization/armed/UNKNOWN — FIXED.** Authorization, unknown telemetry and alarm/fault states are distinct.

### CLEANUP
- **A-006 — Parallel/dead Android architecture — FIXED + CI VALIDATED.** Removed five `ua.homeguard.app.alarm...` source files, `HomeGuardApi`, `DeviceDtos`, `HomeGuardRepository`, `MainUiState`, `MainViewModel`, and old `AlarmAcknowledgementMappingTest`. Total **11 files**, **293 production source lines + 41 test lines = 334 lines removed**. Build #1143 passed on exact cleanup head.
- **A-021 — RegisteredDeviceStore process-global `activeStore` bridge — FIXED.** Removed static registry mutation hooks; app-owned store is passed explicitly to consumers.

## Quantitative audit metrics
Measured only where repository/CI provides real numbers. Phone-only metrics are intentionally not invented.

### Code / structure
- Base: `e522dc7ffc1a716f370ac7050b58c6f1a23e618a`.
- Latest code head before this register update: `ceb62d116ab2578f3503ba0c04b1129bebf0ab7b`.
- Commits ahead of base before this register-only update: **98**.
- PR footprint at that head: **49 changed files, 1,411 additions, 710 deletions** (production + tests + audit docs; not a pure LOC reduction metric).
- Proven-dead island: **11 files / 334 lines removed**, including **293 production lines**.
- `DeviceEndpointResolver`: current PR diff versus base is **1 addition / 25 deletions** after making it read-only.

### APK / CI artifact
- Build #1155 `MyFist-Android` artifact ZIP: **9,611,209 bytes**.
- Extracted `MyFist.apk`: **10,018,339 bytes**.
- A matching exact-base Android artifact was not found, therefore no fabricated before/after APK percentage is reported.

### Runtime-contract numbers visible in code/tests
- Background UDP discovery cadence: **5 s**.
- Discovery freshness: **30 s**; expiry reevaluation tick: **1 s**.
- Registry last-seen persistence throttle: **60 s**; steady timestamp-only upper bound reduced from **12/min → 1/min per continuously discovered controller**.
- HTTP subnet scan parallelism: **32 probes**.
- HTTP discovery response cap: **64 KiB characters**.
- Provisioning Wi-Fi scan response cap: **256,000 bytes** on both HTTP body paths.
- Telemetry reconnect backoff: **2 / 5 / 10 / 20 / 30 s**, capped at 30 s.
- Event history cap: **256 records**; append performs **0 persisted-history reads per new event after store construction** versus 1 full read/parse per append previously.

## Phone benchmark plan after code freeze
Use the same phone, same Android version, same Wi-Fi/AP, same ESP firmware/config, same charger state, and at least 5 runs per measurement. Record median and p95 where practical.

1. **Cold start:** force-stop, clear from recents, launch via `am start -W`; record `ThisTime/TotalTime/WaitTime` for 5-10 runs.
2. **Warm start:** leave process alive and relaunch; record the same timings.
3. **Memory:** after 60 s idle, during active discovery, and live telemetry capture `dumpsys meminfo <package>`: Total PSS/RSS, Java Heap, Native Heap, Graphics/Other.
4. **CPU:** sample `top`/`dumpsys cpuinfo` for 60 s in idle, discovery and telemetry states; compare average and peak.
5. **Discovery latency:** manual scan tap → first matching ESP card and scan complete; repeat 10 times.
6. **Reconnect:** disable/re-enable Wi-Fi and separately reboot/switch ESP; measure OFFLINE → CONNECTED and verify one 2/5/10/20/30 s retry chain.
7. **Lifecycle soak:** 50 foreground/background cycles, 20 device switches, 20 repeated scan taps, Wi-Fi loss/recovery loops; verify one card/controller, no duplicate notifications, no leaked discovery jobs and no stale telemetry.

## Current execution status
- Exact-head Build #1172 passed on `75afd19c36b1bd0ba25d659885874e4b4145e5e4`, validating the A-030..A-036 code/test set.
- Final case-insensitive selection/delete fixes A-037 and Wi-Fi scan memory bound A-038 are now on the audit branch.
- Exact-head Build #1176 for `ceb62d116ab2578f3503ba0c04b1129bebf0ab7b` is pending and must pass before code freeze.
- `main` remains untouched; PR #53 remains draft and unmerged.

## Next immediate blocks
1. Finish exact-head #1176; if red, fix the exact Android/host failure first.
2. One last read-only MainActivity/lifecycle/permission review; no architecture churn unless a concrete defect is proven.
3. Freeze Android code after green exact-head CI.
4. Produce final CI-measured report, then run the phone benchmark once.

## Audit rule
Do not merge to `main` during discovery. Keep all work isolated on `audit/android-full-20260818` / draft PR #53 until cleanup/lifecycle/reconnect is complete, exact-head CI is green, and later phone validation passes.
