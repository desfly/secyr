# HomeGuard-S3 Android full audit — 2026-08-18

Branch: `audit/android-full-20260818`  
Base: `main`  
Draft PR: `#53`

## Goal
Keep one canonical path for discovery, registry, selection, auth/session, provisioning, Factory Reset and backup/restore. Prefer deletion and explicit dependency flow over duplicate runtimes, process-global bridges and screen-owned storage/network state.

## Findings register

### CRITICAL

#### A-001 — Plaintext first-run password — FIXED
First-run password moved from SharedPreferences to `SecureTokenStore` (Android Keystore + AES/GCM), including legacy plaintext migration/removal.

### HIGH

#### A-002 — Discovery dedup was host-based — FIXED
Discovery now reconciles transitive physical-controller identity through `ControllerIdentity.sameController()` with tests.

#### A-003 — Oversized/duplicated Bruce — FIXED
Normal Device List uses only the compact Bruce mark.

#### A-007 — Stale WebSocket frames after device switch — FIXED
`TelemetrySocket` ignores callbacks from sockets that are no longer active.

#### A-008 — Factory Reset network errors misclassified as success — FIXED
Transport loss counts as expected destructive-reset disconnect only after the request body was submitted.

#### A-009 — Device secret reuse/restore leakage — FIXED
API/telemetry secrets are device-bound; normal controller changes clear carried-over secrets.

#### A-010 — Add Device was a prominent primary action — FIXED
Device List exposes Add as a secondary action.

#### A-011 — Manual search progress represented UDP only — FIXED
Coordinator progress combines UDP + HTTP until both finish.

#### A-014 — HTTP discovery retained stale devices after Wi-Fi loss — FIXED
Missing Wi-Fi/IPv4 clears stale HTTP results.

#### A-015 — Duplicate discovery result counters — FIXED
Search status and result count are rendered once each.

#### A-016 — Provisioning stable-ID comparison was case-sensitive — FIXED
ID and HTTPS normalization are case-insensitive; handoff test covers lowercase identity.

#### A-017 — QR provisioning bypassed owner-friendly-name registry contract — FIXED
QR provisioning requires a nonblank owner name, registers the stable controller ID, then refreshes its LAN endpoint.

#### A-018 — ProvisioningScreen owned a second discovery/settings runtime — FIXED ON AUDIT BRANCH
Files:
- `android/app/src/main/java/ua/homeguard/s3/ui/screens/ProvisioningScreen.kt`
- `android/app/src/main/java/ua/homeguard/s3/MainActivity.kt`

Removed the screen-owned `LocalDiscoveryCoordinator`, screen-owned `SettingsStore`, `DisposableEffect` runtime start/stop and `Activity.recreate()` synchronization workaround. `ProvisioningScreen` is now presentation/local-form state only and receives the existing MainActivity-owned discovery state/actions. Already-connected and manual-IP selection now execute through the same application-owned `RegisteredDeviceStore` + `SettingsStore` instances used by the rest of the app.

#### A-019 — HttpDeviceApi cancellation could leak Response — FIXED
Cancellation cancels the OkHttp call; an undeliverable response is closed through the cancellable-continuation resume callback.

#### A-022 — Stale mDNS report could stay online indefinitely — FIXED
All discovery sources use a common 30-second freshness window before deduplication.

#### A-023 — Provisioning shortcuts bypassed registry naming — FIXED
Already-connected/manual-IP paths require owner-friendly name and register before selection. After A-018 they also no longer use a second settings/runtime path.

#### A-024 — Setup AP cancellation could leave process bound to temporary Wi-Fi — FIXED
Cancellation/terminal callback cleanup now unbinds and unregisters safely.

#### A-025 — Rapid repeated manual scan could launch overlapping full discovery passes — FIXED ON AUDIT BRANCH
File:
- `android/app/src/main/java/ua/homeguard/s3/network/LocalDiscoveryCoordinator.kt`

A fast double tap can invoke `rescan()` twice before Compose has time to render the disabled scanning button. Each call previously set the same `manualRescanActive` flag independently while running its own UDP+HTTP pass. One scan could finish first and clear the flag while another scan was still running, making the UI report a completed/idle scan while network work continued and allowing yet another scan to start.

Fix: an atomic single-flight guard now accepts only one manual full scan at a time. Extra overlapping calls return immediately instead of queuing another full UDP+HTTP sweep. The active flag is cleared only by the owning scan in `finally`.

### MEDIUM

#### A-004 — MainActivity is an orchestration god-object — OPEN
It still owns many runtime services and navigation/auth/maintenance state. Decompose only after canonical runtime paths and acceptance contracts are stable; do not add parallel coordinators.

#### A-005 — Global cleartext HTTP enabled — OPEN
`android:usesCleartextTraffic="true"` is wider than the local/setup paths that need HTTP. Constrain after endpoint audit without breaking ESP setup/bench use.

#### A-012 — Selected controller comparison case-sensitive — FIXED
Selected ID matching is case-insensitive.

#### A-013 — Legacy unnamed devices synthesized `HomeGuard` — FIXED
Unnamed entries remain unnamed until owner rename; UI shows `Потрібна назва`.

#### A-020 — Device-state picons mixed authorization/armed state and UNKNOWN/fault — FIXED
Authorization, unknown telemetry and actual fault/alarm are rendered distinctly.

### CLEANUP

#### A-006 — Parallel/dead Android architecture — OPEN, SCOPE CONFIRMED
Obsolete island includes at least:
- `ua.homeguard.app.alarm...`
- `ua.homeguard.s3.api...`
- `ua.homeguard.s3.data...`
- `ua.homeguard.s3.ui.main.MainUiState/MainViewModel`

PR #51 removed only part of this island and failed compile because `ui/main` still referenced deleted DTO/repository classes. Do not cherry-pick it. Remove the complete proven-dead island atomically after source/test call-graph verification.

#### A-021 — RegisteredDeviceStore process-global `activeStore` bridge — FIXED ON AUDIT BRANCH
Files:
- `android/app/src/main/java/ua/homeguard/s3/storage/RegisteredDeviceStore.kt`
- `android/app/src/main/java/ua/homeguard/s3/repository/ProvisioningCoordinator.kt`
- `android/app/src/main/java/ua/homeguard/s3/network/DeviceSession.kt`
- `android/app/src/main/java/ua/homeguard/s3/MainActivity.kt`

Removed `activeStore` and all static `markActive* / reconcileActive* / refreshActive* / registerActive / removeActive` methods. `RegisteredDeviceStore` is now explicitly injected into `ProvisioningCoordinator` and `DeviceSession`; both call the same app-owned store instance directly. This removes process-global hidden state and makes registry mutation paths explicit.

## Confirmed positives
- API/telemetry tokens use Android Keystore-backed secure storage.
- New registry entries require an owner-friendly name.
- Device List has rename/delete/properties, unauthorized state and one-card-per-controller reconciliation.
- Factory Reset client requires explicit destructive confirmation.
- Build #1119 passed on head `d9dd97029301413d406a5f72ccea87e9fadbe816` before the runtime/bridge cleanup.

## Current execution status
Current code head before this register update: `d04db9b2ca9eb9bf76e8d51288830241038fe50e`.

Runtime/bridge/loop cleanup performed in the latest passes:
- `DeviceSession` receives `RegisteredDeviceStore` explicitly;
- `ProvisioningCoordinator` receives `RegisteredDeviceStore` explicitly;
- `DeviceEndpointResolver` receives the same registry explicitly;
- global `RegisteredDeviceStore.activeStore` bridge removed;
- duplicate `ProvisioningScreen` discovery/settings runtime removed;
- `Activity.recreate()` removed from provisioning selection;
- provisioning screen consumes MainActivity-owned discovery state/actions;
- manual/already-connected shortcut selection uses the canonical app-owned registry/settings path;
- overlapping manual full rescans are suppressed with a single-flight guard.

Build #1128 is running for head `d04db9b2ca9eb9bf76e8d51288830241038fe50e`. Do not treat A-025 as CI-validated until this run succeeds.

## Next immediate blocks
1. Check CI for the latest head and fix the exact failing job first if red.
2. Finish call-graph proof for the obsolete `ui/main + api/data/alarm` island, including tests, then delete atomically if proven dead.
3. Continue lifecycle/coroutine review around NSD, network callbacks and telemetry, especially repeated start/stop/reconnect paths.
4. Audit MainActivity for further removable orchestration chains before introducing any new abstraction.
5. Audit manifest/build/security surface, especially global cleartext and permission timing.
6. Strengthen acceptance tests around provisioning selection, repeated scan taps and one-controller-one-card behavior.

## Audit rule
Do not merge to `main` during discovery. Keep changes isolated on the audit branch and merge only verified minimal changes after CI and phone validation.
