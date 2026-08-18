# HomeGuard-S3 Android full audit — 2026-08-18

Branch: `audit/android-full-20260818`  
Base: `main`  
Draft PR: `#53`

## Goal
Keep one canonical path for discovery, registry, selection, auth/session, provisioning, telemetry/reconnect, Factory Reset and backup/restore. Functional correctness and stable lifecycle behavior come first; code reduction follows only when behavior is preserved.

## Findings register

### CRITICAL

#### A-001 — Plaintext first-run password — FIXED
First-run password moved from SharedPreferences to Android Keystore-backed `SecureTokenStore`, including legacy plaintext migration/removal.

### HIGH

#### A-002 — Discovery dedup was host-based — FIXED
Discovery now reconciles transitive physical-controller identity through `ControllerIdentity.sameController()`.

#### A-003 — Oversized/duplicated Bruce — FIXED
Normal Device List uses only the compact Bruce mark.

#### A-007 — Stale WebSocket frames after device switch — FIXED
`TelemetrySocket` ignores callbacks from sockets that are no longer active.

#### A-008 — Factory Reset network errors misclassified as success — FIXED
Transport loss counts as expected destructive-reset disconnect only after the request body was submitted.

#### A-009 — Device secret reuse/restore leakage — FIXED
API/telemetry secrets are device-bound; controller changes clear carried-over secrets.

#### A-010 — Add Device was a prominent primary action — FIXED
Device List exposes Add as a secondary action.

#### A-011 — Manual search progress represented UDP only — FIXED
Coordinator progress combines UDP + HTTP until both finish.

#### A-014 — HTTP discovery retained stale devices after Wi-Fi loss — FIXED
Missing Wi-Fi/IPv4 clears stale HTTP results.

#### A-015 — Duplicate discovery result counters — FIXED
Search state/result count are rendered once.

#### A-016 — Provisioning stable-ID comparison was case-sensitive — FIXED
Stable IDs and HTTPS handling are normalized case-insensitively.

#### A-017 — QR provisioning bypassed owner-friendly-name contract — FIXED
QR provisioning requires an owner name and registers the stable controller ID before endpoint refresh.

#### A-018 — ProvisioningScreen owned a second discovery/settings runtime — FIXED
Removed the screen-owned `LocalDiscoveryCoordinator`, screen-owned `SettingsStore`, `DisposableEffect` start/stop and `Activity.recreate()` workaround. The screen now consumes MainActivity-owned discovery state/actions.

#### A-019 — HttpDeviceApi cancellation could leak Response — FIXED
Cancellation cancels the OkHttp call and closes an undeliverable response.

#### A-022 — Stale discovery could remain visible without a new source emission — FIXED + CI VALIDATED
All discovery sources use one 30-second freshness window before deduplication, and `LocalDiscoveryCoordinator` drives expiry with a 1-second clock tick. Previously the freshness predicate only re-ran when mDNS/UDP/HTTP emitted; if Android omitted `onServiceLost` and no other source changed, a stale device could remain visible indefinitely despite the nominal 30-second window. Build #1153 passed on head `8d67803d93c49af71fc42ed856daae91f94dcc00`.

#### A-023 — Provisioning shortcuts bypassed registry naming — FIXED
Already-connected/manual-IP paths require owner-friendly name and use the canonical app-owned registry/settings path.

#### A-024 — Setup AP cancellation could leave process bound to temporary Wi-Fi — FIXED
Cancellation and terminal callback cleanup now unbind/unregister safely.

#### A-025 — Rapid repeated manual scan could launch overlapping full scans — FIXED
`LocalDiscoveryCoordinator.rescan()` is single-flight; overlapping taps no longer start parallel UDP+HTTP sweeps or clear scan state early.

#### A-026 — Telemetry stayed offline forever after transient WebSocket failure — FIXED
`DeviceSession` owns one reconnect job with 2/5/10/20/30-second capped backoff. CONNECTED, UNAUTHORIZED, target change and stop cancel retries; stale targets cannot reconnect.

#### A-027 — Synchronous telemetry connect setup failure could kill DeviceSession collector — FIXED + CI VALIDATED
Files:
- `android/app/src/main/java/ua/homeguard/s3/network/TelemetrySocket.kt`
- `android/app/src/test/java/ua/homeguard/s3/network/TelemetrySocketConnectFailureTest.kt`

Before a WebSocket exists, URL parsing, certificate-pin validation or client construction can throw synchronously. Previously that exception escaped `TelemetrySocket.connect()` into the `DeviceSession` Flow collector, killing the target coroutine. A later settings/device change would then have no collector left to reconnect.

Fix: connect setup is contained inside `TelemetrySocket`; synchronous setup failures now move the socket to `OFFLINE` instead of escaping. Tests cover malformed URL and invalid certificate pin. Build #1146 passed on head `b9f31a0a4e7052daf709a052471f26d3e62f0523`.

#### A-028 — Invalid manual IP closed provisioning despite failed validation — FIXED + CI VALIDATED
File:
- `android/app/src/main/java/ua/homeguard/s3/MainActivity.kt`

The provisioning callback previously set `provisioningOpen=false` unconditionally after calling `addManualDevice()`. If address validation rejected the IP, the device was not added but the provisioning screen still closed. The close transition now lives only in the successful canonical `addManualDevice()` path, after validation and registry/settings update. Build #1153 passed on head `8d67803d93c49af71fc42ed856daae91f94dcc00`.

#### A-029 — NSD start/restart races could poison discovery lifecycle — FIXED + CI VALIDATED
File:
- `android/app/src/main/java/ua/homeguard/s3/network/NsdDeviceDiscovery.kt`

Two lifecycle holes were present. First, `start()` set `started=true` before `NsdManager.discoverServices()`, so a synchronous platform exception could permanently suppress later retries. Second, checking only `started` on resolve callbacks was insufficient across a rapid stop→start cycle: an in-flight resolve from the previous run could arrive after the new start, see `started=true`, and republish a stale endpoint into the new discovery session.

Fix: synchronous start failure rolls the flag back, callback-visible state is volatile, and every discovery run now gets a monotonically increasing generation. Resolve callbacks capture their generation and are ignored unless it still matches the active run. Stop and failed start invalidate all outstanding generations. Build #1155 passed on exact head `61d6b3094d723af6f6e7b768dac41b42602c2df4`.

#### A-030 — Discovery refresh caused repeated persistent registry writes — FIXED ON AUDIT BRANCH
Files:
- `android/app/src/main/java/ua/homeguard/s3/storage/RegisteredDeviceStore.kt`
- `android/app/src/test/java/ua/homeguard/s3/storage/RegisteredDeviceRefreshPolicyTest.kt`

UDP discovery runs every 5 seconds. The MainActivity discovery collector refreshes registered devices when fresh reports arrive, and `refreshDiscovered()` previously persisted the complete SharedPreferences registry whenever `lastSeenAtMs` changed. With one continuously visible controller this could produce up to 12 timestamp-only registry writes per minute (about 720/hour), plus matching `_devices` emissions and UI recomposition pressure.

Fix: stable identity/endpoint refreshes persist `lastSeenAtMs` at a 60-second granularity; real device-ID or base-URL changes still persist immediately. Live online/offline state remains driven by `discovered`, so discovery frequency and availability behavior are unchanged. The timestamp-only persistent-write upper bound is reduced approximately 12× (12/minute → 1/minute per continuously discovered controller). Three unit tests cover the throttle boundary, immediate endpoint changes and case-insensitive stable IDs. CI is pending for this newest head.

### MEDIUM

#### A-004 — MainActivity is an orchestration god-object — OPEN
Decompose only after canonical paths and lifecycle contracts are stable; do not add parallel coordinators.

#### A-005 — Global cleartext HTTP enabled — OPEN
`android:usesCleartextTraffic="true"` is wider than the local/setup paths that need HTTP. Constrain only after endpoint audit without breaking ESP setup/bench access.

#### A-012 — Selected controller comparison case-sensitive — FIXED
Selected ID matching is case-insensitive.

#### A-013 — Legacy unnamed devices synthesized `HomeGuard` — FIXED
Unnamed entries remain unnamed until owner rename; UI shows `Потрібна назва`.

#### A-020 — Device-state picons mixed authorization/armed/UNKNOWN — FIXED
Authorization, unknown telemetry and real alarm/fault states are rendered distinctly.

### CLEANUP

#### A-006 — Parallel/dead Android architecture — FIXED
The complete proven-dead island was removed atomically after source/test call-graph verification:
- five `ua.homeguard.app.alarm...` source files;
- `ua.homeguard.s3.api.HomeGuardApi`;
- `ua.homeguard.s3.api.model.DeviceDtos`;
- `ua.homeguard.s3.data.HomeGuardRepository`;
- `ua.homeguard.s3.ui.main.MainUiState`;
- `ua.homeguard.s3.ui.main.MainViewModel`;
- old `AlarmAcknowledgementMappingTest`.

Total: **11 files removed**. Build #1143 passed on exact cleanup head `d54e01083bcf88e1f8684e1af7dbe6c7c92cfdd3`, proving the island was not required by the active Android build/test graph.

#### A-021 — RegisteredDeviceStore process-global `activeStore` bridge — FIXED
Removed `activeStore` and static registry mutation helpers. `ProvisioningCoordinator`, `DeviceSession` and `DeviceEndpointResolver` receive the same app-owned `RegisteredDeviceStore` explicitly.

## Confirmed positives
- API/telemetry tokens use Android Keystore-backed secure storage.
- New registry entries require owner-friendly names.
- Device List has rename/delete/properties, unauthorized state and one-controller-one-card reconciliation.
- Factory Reset requires explicit destructive confirmation.
- Build #1143 passed after the 11-file dead-architecture removal.
- Build #1146 passed after A-027 telemetry setup-failure containment and tests.
- Build #1153 passed after active stale-discovery expiry, manual-IP provisioning correction, host-contract update and initial NSD lifecycle hardening.
- Build #1155 passed after the strengthened NSD generation guard.

## Quantitative audit metrics
Measured from the PR/base and exact CI artifacts; phone-only runtime metrics are deliberately not invented.

- PR delta after A-030 tests, before this register-only commit: 42 changed files, 1,285 additions, 670 deletions; this includes production code, tests and this audit document.
- Proven-dead architecture removed: 11 files.
- Current exact-CI Android artifact from Build #1155: `MyFist-Android` ZIP = 9,611,209 bytes; extracted `MyFist.apk` = 10,018,339 bytes.
- Periodic UDP discovery cadence: one pass every 5 seconds.
- A-030 timestamp-only registry persistence: theoretical maximum reduced from 12 writes/minute to 1 write/minute per continuously discovered controller while preserving immediate ID/base-URL updates.
- Telemetry reconnect schedule: 2 / 5 / 10 / 20 / 30-second capped backoff.
- Phone measurements still required after code freeze: cold/warm startup, PSS/RSS, Java/native heap, CPU in idle/discovery/telemetry, discovery latency and real reconnect time.

## Current execution status
Latest code/test head before this register update: `6cb00edd27df1b4a82616f90e3a529109c940e58`.

Latest pass:
- confirmed Build #1155 success on exact previous head `61d6b3094d723af6f6e7b768dac41b42602c2df4`, therefore A-029 is now CI validated;
- found A-030 persistent-write/recomposition churn in the registry refresh path;
- throttled timestamp-only persistence to 60 seconds while keeping real endpoint/identity changes immediate;
- added three focused JVM unit tests for the refresh policy;
- captured the current Build #1155 artifact and measured the actual extracted APK at 10,018,339 bytes.

CI for the A-030 code/test head is pending and must pass before A-030 is marked CI validated.

## Next immediate blocks
1. Check A-030 exact-head CI and fix the precise failure first if red.
2. Continue the last MainActivity/network lifecycle pass for repeated writes/collectors/state transitions.
3. Audit manifest/build/security surface, especially global cleartext and permission timing without breaking local/setup HTTP.
4. Freeze code after the final green exact-head CI and produce the benchmark/acceptance procedure for phone measurements.
5. After code freeze only: compare cold/warm startup, PSS/RSS, heaps, CPU, discovery latency and reconnect on the same phone/build conditions.

## Audit rule
Do not merge to `main` during discovery. Keep changes isolated on the audit branch and merge only verified minimal changes after CI and later phone validation.
