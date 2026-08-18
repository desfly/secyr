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

#### A-022 — Stale discovery could remain visible without a new source emission — FIXED ON AUDIT BRANCH
All discovery sources use one 30-second freshness window before deduplication, and `LocalDiscoveryCoordinator` now drives expiry with a 1-second clock tick. Previously the freshness predicate only re-ran when mDNS/UDP/HTTP emitted; if Android omitted `onServiceLost` and no other source changed, a stale device could remain visible indefinitely despite the nominal 30-second window.

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

#### A-028 — Invalid manual IP closed provisioning despite failed validation — FIXED ON AUDIT BRANCH
File:
- `android/app/src/main/java/ua/homeguard/s3/MainActivity.kt`

The provisioning callback previously set `provisioningOpen=false` unconditionally after calling `addManualDevice()`. If address validation rejected the IP, the device was not added but the provisioning screen still closed. The close transition now lives only in the successful canonical `addManualDevice()` path, after validation and registry/settings update.

#### A-029 — NSD start failure/stale resolve could poison discovery lifecycle — FIXED ON AUDIT BRANCH
File:
- `android/app/src/main/java/ua/homeguard/s3/network/NsdDeviceDiscovery.kt`

`start()` previously set `started=true` before `NsdManager.discoverServices()`. A synchronous platform exception could therefore leave discovery permanently marked started and suppress all future retries. Also an in-flight resolve callback could arrive after `stop()` and republish a device into the stopped runtime. Start now rolls the flag back on synchronous failure, `started` is visible across callback threads, and late found/resolve callbacks are ignored after stop.

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

## Current execution status
Latest code head before this register update: `ca2c0e9ac44c0268e827dcbf93fbee1245162e65`.

Latest pass:
- Build #1148 showed Android APK/tests green but failed the host discovery contract because that script matched the old literal three-input `combine(...)` form; the product code now correctly has a fourth freshness-clock input;
- updated `tools/check_android_discovery_contract.py` to inspect the actual `devices` combine inputs instead of matching one formatting string;
- fixed A-028 so invalid manual-IP input no longer closes provisioning;
- fixed A-029 so synchronous NSD start failure cannot permanently suppress retries and stale resolve callbacks do not republish after stop;
- no new runtime/store/coordinator was introduced.

CI for the latest head is pending. Do not mark A-022/A-028/A-029 fully CI-validated until an exact-head run succeeds.

## Next immediate blocks
1. Check latest CI and fix the exact failing job first if red.
2. Continue lifecycle review around network callbacks and repeated start/stop/reconnect paths.
3. Audit `MainActivity` for removable orchestration chains without adding abstraction layers.
4. Audit manifest/build/security surface, especially global cleartext and permission timing.
5. Strengthen acceptance tests around restart, device switching, network loss/recovery, invalid inputs and repeated user actions.

## Audit rule
Do not merge to `main` during discovery. Keep changes isolated on the audit branch and merge only verified minimal changes after CI and later phone validation.
