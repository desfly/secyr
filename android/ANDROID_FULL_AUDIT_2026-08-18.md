# HomeGuard-S3 Android full audit — 2026-08-18

Branch: `audit/android-full-20260818`  
Base: `main` @ `e522dc7ffc1a716f370ac7050b58c6f1a23e618a`  
Draft PR: `#53`

## Goal
One canonical runtime path for discovery, registry, selection, auth/session, provisioning, telemetry/reconnect, Factory Reset and backup/restore. Correctness and lifecycle stability first; simplification and measurable resource reduction second. `main` stays untouched and this PR stays draft/unmerged until code freeze + phone validation.

## Findings register

### CRITICAL
- **A-001 — Plaintext first-run password — FIXED.** First-run password moved to Android Keystore-backed `SecureTokenStore`, including legacy plaintext migration/removal.

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
- **A-022 — Stale discovery could remain visible without a new source emission — FIXED + CI VALIDATED.** One 30-second freshness rule plus active expiry tick. Build #1153 passed.
- **A-023 — Provisioning shortcuts bypassed registry naming — FIXED.** Already-connected/manual-IP paths require friendly name and canonical registry/settings path.
- **A-024 — Setup AP cancellation could leave process bound to temporary Wi-Fi — FIXED.** Terminal/cancel cleanup unbinds/unregisters safely.
- **A-025 — Rapid repeated manual scan could launch overlapping full scans — FIXED.** `rescan()` is single-flight.
- **A-026 — Telemetry stayed offline forever after transient WebSocket failure — FIXED.** One reconnect job with 2/5/10/20/30-second capped backoff; target changes/stop/CONNECTED/UNAUTHORIZED cancel retries.
- **A-027 — Synchronous telemetry connect setup failure could kill DeviceSession collector — FIXED + CI VALIDATED.** Malformed URL/pin setup failures become `OFFLINE`; collector survives. Build #1146 passed.
- **A-028 — Invalid manual IP closed provisioning — FIXED + CI VALIDATED.** Screen closes only after successful validation + registry/settings update. Build #1153 passed.
- **A-029 — NSD start/restart races could poison discovery lifecycle — FIXED + CI VALIDATED.** Start rollback + callback generation gate prevent old resolve callbacks contaminating later runs. Build #1155 passed for the first hardening stage.
- **A-030 — Discovery refresh caused repeated persistent registry writes — FIXED + CI VALIDATED.** UDP runs every 5 s; timestamp-only registry persistence is throttled to 60 s while ID/base-URL changes persist immediately. Steady timestamp writes drop from up to 12/minute to 1/minute per controller (~12×). Build #1172 passed.
- **A-031 — Duplicate telemetry sequence could trigger duplicate history write/notification — FIXED + CI VALIDATED.** Duplicate event identity is suppressed before history/notification fan-out. Build #1172 passed; later multi-controller hardening makes identity controller-aware.
- **A-032 — No-op settings/registry mutations wrote SharedPreferences/secure storage and emitted Flow state — FIXED + CI VALIDATED.** Stores return early when normalized state/list is unchanged. Build #1172 passed.
- **A-033 — HTTP subnet discovery accepted unbounded 200-OK bodies — FIXED + CI VALIDATED.** Discovery JSON is capped at 64 KiB characters. Build #1172 passed.
- **A-034 — DeviceEndpointResolver duplicated registry/settings reconciliation side effects — FIXED + CI VALIDATED.** Resolver is read-only; one MainActivity-owned reconciliation collector remains. Build #1172 passed.
- **A-035 — Event history reparsed all persisted JSON on each event — FIXED + CI VALIDATED.** History loads once into synchronized memory cache; duplicate identity is zero-write. Build #1172 passed.
- **A-036 — Parallel HTTP scan progress could regress due to out-of-order worker updates — FIXED + CI VALIDATED.** Only the short progress update is serialized; 32 HTTP probes remain parallel. Build #1172 passed.
- **A-037 — Case-only device-ID change could clear route/certificate or survive deletion as selected — FIXED.** Device selection/delete matching is case-insensitive; test covers `HG-AbC123` → `hg-abc123`.
- **A-038 — Wi-Fi scan body cap missing for Content-Length responses — FIXED.** Both body paths are capped at 256,000 bytes.
- **A-039 — Discovery work continued while Activity was backgrounded — FIXED.** Periodic mDNS/UDP/freshness work now follows `onStart()`/`onStop()` instead of running until `onDestroy()`. Telemetry/session is intentionally not stopped, preserving monitoring/notifications.
- **A-040 — Old NSD DiscoveryListener callback could mutate a newer discovery generation — FIXED.** Every start owns a distinct listener; `found`, `lost`, and `resolve` callbacks are generation-gated. An old `onServiceLost()` can no longer remove a freshly resolved endpoint after stop→start. Three unit tests cover active, stale-generation and stopped callback gates.
- **A-041 — Repeated provisioning tap could run two Setup-AP/token/settings flows — FIXED.** Provisioning is single-flight via an atomic gate released in `finally`; focused test verifies the second acquisition is rejected until release.
- **A-042 — Stale login could attach an old controller telemetry token/session after device switch — FIXED.** Login now re-validates selected controller after login response, after telemetry-session response, and before returning. Three tests cover case-insensitive same-device, different-device and blank-selection cases.
- **A-043 — Event sequence identity collided across different controllers — FIXED.** Event dedup/history/notifications carry controller identity, so `sequence=42` from HG-A and HG-B are separate events while same-controller duplicates remain suppressed. Test explicitly preserves equal sequence across two controller IDs.

### MEDIUM
- **A-004 — MainActivity remains a large orchestration object — OPEN / DEFERRED.** Do not mechanically split it before runtime contracts are frozen; adding layers now risks recreating the parallel runtimes just removed.
- **A-005 — Global cleartext HTTP enabled — OPEN / CURRENTLY REQUIRED BY CONTRACT.** Setup/manual/fallback discovery intentionally talks HTTP to arbitrary LAN IPv4 targets; Android domain allowlists cannot safely express arbitrary manual/RFC1918 IPs. Revisit only with protocol/endpoint redesign.
- **A-012 — Selected-controller comparison case-sensitive — FIXED.**
- **A-013 — Legacy unnamed devices synthesized `HomeGuard` — FIXED.** UI requires owner rename and shows `Потрібна назва`.
- **A-020 — Device picons mixed authorization/armed/UNKNOWN — FIXED.** Authorization, unknown telemetry and alarm/fault states are distinct.
- **A-044 — `autoReconnect` persistence without behavior — FIXED.** `DeviceSession` includes `autoReconnect` in session target, cancels retry when disabled, and resumes retry only when enabled while OFFLINE. Connection identity itself remains endpoint+token so toggling policy does not force a needless socket reconnect.

### CLEANUP
- **A-006 — Parallel/dead Android architecture — FIXED + CI VALIDATED.** Removed five `ua.homeguard.app.alarm...` files, `HomeGuardApi`, `DeviceDtos`, `HomeGuardRepository`, `MainUiState`, `MainViewModel`, and old `AlarmAcknowledgementMappingTest`: **11 files / 334 lines**, including **293 production lines**. Build #1143 passed.
- **A-021 — RegisteredDeviceStore process-global `activeStore` bridge — FIXED.** Static registry mutation hooks removed; the app-owned store is passed explicitly.

## Quantitative audit metrics
Only measured repository/CI numbers are reported. Phone-only performance numbers are intentionally not invented.

### Code / structure
- Base: `e522dc7ffc1a716f370ac7050b58c6f1a23e618a`.
- Code/test head before this register update: `9736bb54d0ca6cbb6257dab9d60d9c17ea09c9c7`.
- At that head PR #53 was **119 commits ahead**, **56 changed files**, **1,788 additions / 832 deletions** across production, tests and audit material. These totals are not a pure production-LOC metric.
- Proven-dead island removed: **11 files / 334 lines**, including **293 production lines**.
- `DeviceEndpointResolver` was reduced to read-only resolution; the duplicate write/reconcile branch was removed.

### APK / CI artifacts
- Build #1155 `MyFist-Android` artifact ZIP: **9,611,209 bytes**.
- Extracted `MyFist.apk`: **10,018,339 bytes**.
- No matching exact-base Android artifact was found, therefore no fake before/after APK percentage is reported.

### Runtime-contract numbers
- Foreground periodic UDP discovery cadence: **5 s**.
- Background Activity state: periodic mDNS/UDP/freshness work **stopped**; session/telemetry intentionally remains available.
- Discovery freshness: **30 s**, foreground expiry tick **1 s**, resume grace **3 s** before expiring cached reports.
- Registry last-seen persistence throttle: **60 s**; timestamp-only upper bound **12/min → 1/min/controller**.
- Manual scan: **single-flight**; HTTP subnet scan up to **32 probes** in parallel.
- HTTP discovery body cap: **64 KiB characters**.
- Provisioning Wi-Fi scan body cap: **256,000 bytes** on both body paths.
- Provisioning handoff rediscovery: bounded child scan loop, **5 s cadence**, cancelled when device is found or timeout/coroutine ends; it no longer stop/starts the app-wide discovery runtime.
- Provisioning submit: **one active run maximum**; repeated tap while active is rejected.
- Telemetry reconnect backoff: **2 / 5 / 10 / 20 / 30 s**, capped at 30 s; `autoReconnect=false` disables retry.
- Event history cap: **256 records**; after store construction append performs **0 persisted-history reads** versus one full persisted JSON read/parse per append previously.
- NSD callback policy: one listener generation per start; stale-generation callbacks accepted: **0 by contract**.

## CI status chronology relevant to current code
- Build #1143: dead-code island removal green.
- Build #1146: telemetry setup-failure containment green.
- Build #1153: stale-discovery expiry/manual-IP/lifecycle stage green.
- Build #1155: NSD first hardening stage green.
- Build #1172: A-030..A-036 code/test set green.
- Build #1177: exact head `1704247e...` green before the final lifecycle/device-switch pass.
- Build #1186: host validation caught a **test-contract mismatch**, not a production discovery regression: the Python contract required the literal old `resolve(serviceInfo)` signature and rejected the new generation-aware `resolve(serviceInfo, generation)`. The contract was changed to validate semantics while still requiring an independent `ResolveListener` and absence of the old shared resolver.
- Build #1196 on code/test head `9736bb54...`: Host validation has passed, including Android LAN discovery contract; Android unit/build step has passed and installer preparation has passed. Full workflow completion still depends on its unrelated firmware job at the time of this register update.

## Phone benchmark plan after Android code freeze
Use the same phone, Android version, Wi-Fi/AP, ESP firmware/config and charger state. Run at least 5 samples; report median and p95 where practical.

1. **Cold start:** force-stop, clear from recents, launch via `am start -W`; record `ThisTime`, `TotalTime`, `WaitTime` for 5–10 runs.
2. **Warm start:** leave process alive and relaunch; capture the same timings.
3. **Memory:** after 60 s idle, during active discovery and during live telemetry run `dumpsys meminfo <package>`; capture Total PSS/RSS, Java Heap, Native Heap, Graphics/Other.
4. **CPU:** sample `top`/`dumpsys cpuinfo` for 60 s in idle, discovery and telemetry states; compare average and peak.
5. **Discovery latency:** manual scan tap → first matching ESP card and → scan completion; repeat 10 times.
6. **Reconnect:** Wi-Fi disable/re-enable and separate ESP reboot/switch; measure OFFLINE → CONNECTED and verify one 2/5/10/20/30 s retry chain, not overlapping retries.
7. **Lifecycle soak:** 50 foreground/background cycles, 20 device switches, 20 repeated scan taps, repeated Wi-Fi loss/recovery; verify one card/controller, no duplicate notifications, no leaked discovery jobs, no stale telemetry and no cross-device token/session contamination.
8. **Provisioning repeated-action soak:** repeatedly double-tap provisioning confirmation around the enabled→busy transition; verify only one Setup-AP request/run is active.

## Remaining before freeze
1. Obtain a green exact-head CI after this audit-register update; if red, fix the exact Android/host failure first.
2. Final read-only review of destructive Factory Reset in-flight behavior and remaining permission/cleartext constraints; no architecture churn without a proven defect.
3. Freeze Android code when exact-head CI is green and no new concrete lifecycle/reconnect defect remains.
4. Only then run the phone benchmark plan and fill real startup/memory/CPU/discovery/reconnect numbers.

## Audit rule
Do not merge to `main` during audit. Keep all work isolated on `audit/android-full-20260818` / draft PR #53 until cleanup/lifecycle/reconnect is complete, exact-head CI is green, and phone validation passes.
