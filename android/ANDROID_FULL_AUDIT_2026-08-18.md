# HomeGuard-S3 Android full audit — 2026-08-18

Branch: `audit/android-full-20260818`  
Base: `main`  
Draft PR: `#53`

## Goal
Complete the Android audit before broad refactoring. Preserve the cemented device-list/discovery/auth/factory-reset behavior; classify findings as `CRITICAL`, `HIGH`, `MEDIUM`, or `CLEANUP`; make only verified minimal changes and keep `main` untouched until CI and phone validation pass.

## Day plan
- repository/startup/navigation/build/security baseline;
- discovery: mDNS/UDP/HTTP/manual, dedup and freshness;
- registry/friendly-name/selection/rename/delete/revoked access;
- Device List/Add Device contract;
- auth/session/credential storage;
- provisioning/Wi-Fi permissions/first-run;
- telemetry/commands/endpoint routing;
- Factory Reset/backup/restore;
- lifecycle/coroutines/background/foreground;
- test/acceptance coverage;
- duplicate/dead architecture including PR #51;
- manifest/build/security surface;
- consolidate and prioritize minimal fixes.

## Findings register

### CRITICAL

#### A-001 — Plaintext first-run password in SharedPreferences — FIXED
Password storage moved to `SecureTokenStore` (Android Keystore + AES/GCM), including migration/removal of the legacy plaintext key.

### HIGH

#### A-002 — Discovery deduplication grouped by host instead of physical controller identity — FIXED
Transitive reconciliation now uses `ControllerIdentity.sameController()` and preserves stable `HG-*` identity. Unit tests cover same-ID/different-host, setup/stable identity, transitive merge, unrelated controllers and source priority.

#### A-003 — Oversized/duplicated Bruce in Device List — FIXED
Normal Device List uses the compact Bruce mark; the separate oversized full-width artwork was removed.

#### A-007 — Stale WebSocket frames could overwrite a newly selected device — FIXED
`TelemetrySocket` ignores callbacks from a socket that is no longer the active socket.

#### A-008 — Any Factory Reset IOException was treated as expected reboot — FIXED
Transport loss is accepted as an expected destructive-reset disconnect only after the request body was actually submitted.

#### A-009 — Restore/device switching could erase or reuse another controller's API/telemetry secret — FIXED
Secrets are device-bound; same-device restore preserves omitted local secrets and controller changes clear carried-over secrets unless provisioning installs a new valid token.

#### A-010 — Add Device was a prominent primary action — FIXED
Normal Device List now exposes only a small secondary Add entry; the Add flow itself is not redesigned.

#### A-011 — Manual scan progress represented UDP only — FIXED
Coordinator progress combines UDP + HTTP and remains active until both branches complete. Tests cover partial/final/error/background states.

#### A-014 — HTTP discovery kept stale devices after Wi-Fi disappeared — FIXED
Missing Wi-Fi/IPv4 clears stale HTTP results and reports a concrete scan error.

#### A-015 — Discovery UI rendered duplicate result counters — FIXED
Search state and result count are rendered once each.

#### A-016 — Provisioning handoff compared controller IDs case-sensitively — FIXED
Stable IDs and HTTPS scheme handling are normalized case-insensitively; pure test covers lowercase discovered identity.

#### A-017 — QR provisioning could bypass owner-friendly-name contract — FIXED FOR QR PATH
QR provisioning requires a nonblank owner label, caps it at 40 chars, disables provisioning until supplied, registers the stable ID with that name and refreshes its LAN endpoint after reboot.

#### A-018 — ProvisioningScreen creates a second discovery/settings runtime inside Compose — OPEN
`MainActivity` already owns `LocalDiscoveryCoordinator` and `SettingsStore`, but the top-level `ProvisioningScreen` creates another pair, starts another discovery runtime and forces `Activity.recreate()` after selection so the primary store observes the secondary write.

Action: route the screen through the existing application runtime and remove the screen-owned discovery/settings pair without changing provisioning UX.

#### A-019 — HttpDeviceApi coroutine cancellation could leak an unclaimed OkHttp Response — FIXED
Cancellation cancels the call; public cancellable-continuation `resume(response) { ... }` closes a response that cannot be delivered, while successful delivery remains owned by the caller's `.use` block.

#### A-022 — Stale mDNS reports could keep a disappeared controller online indefinitely — FIXED
All discovery sources are filtered by a common 30-second freshness window before deduplication. A boundary test covers fresh vs stale reports.

#### A-023 — Provisioning “already on Wi-Fi” / manual-IP shortcuts bypass registry naming — FIXED ON AUDIT BRANCH
Files:
- `android/app/src/main/java/ua/homeguard/s3/ui/screens/ProvisioningScreen.kt`
- `android/app/src/test/java/ua/homeguard/s3/ui/screens/ProvisioningShortcutPolicyTest.kt`

The shortcut paths now require a nonblank owner-friendly name before they are enabled. Before selection, discovered devices and manual-IP entries are registered through the active owner-named registry path; only then is `SettingsStore` selection applied. A focused policy test locks the friendly-name, busy-state and manual-address gates.

Remaining architectural debt: this narrow fix still uses the existing process-global `RegisteredDeviceStore.activeStore` bridge and screen-owned `SettingsStore`; A-018/A-021 must remove that bridge/recreate pattern rather than expanding it further.

#### A-024 — Setup AP cancellation could leave the whole process bound to temporary Wi-Fi — FIXED
Terminal callbacks are guarded; cancelled/unclaimed `BoundSetupNetwork` values close/unbind themselves; cancellation unregisters the network callback; bind failures clean up before propagating.

### MEDIUM

#### A-004 — MainActivity is an orchestration god-object — OPEN
It owns discovery, settings, registry, event history, routing, provisioning, telemetry, session, commands, notifications, navigation state, operator/auth state, backup/restore, QR and Factory Reset.

Action: decompose only after behavior contracts are locked.

#### A-005 — Global cleartext HTTP is enabled for the whole app — OPEN
`android:usesCleartextTraffic="true"` broadens cleartext access beyond the local/setup controller paths that actually need it.

Action: constrain the network security policy without breaking local bench/setup access.

#### A-012 — Selected controller ID comparison was case-sensitive — FIXED
Local selected-ID matching is case-insensitive.

#### A-013 — Legacy unnamed registry entries synthesized visible `HomeGuard` — FIXED
Storage no longer invents a normal-card name. Unnamed legacy devices show `Потрібна назва` and route taps to rename; ID/IP are not substituted as the visible owner name.

#### A-020 — Device-state picons conflated authorization with armed state and treated UNKNOWN as a fault — FIXED
Authorization renders as `🛡 доступ`; no/UNKNOWN snapshot is neutral; actual alarm/fault states remain error-colored; labels are constrained for narrow screens.

### CLEANUP

#### A-006 — Parallel/dead Android architecture is larger than PR #51 originally removed — OPEN, SCOPE CONFIRMED
Known old families:
- `ua.homeguard.app.alarm...`;
- `ua.homeguard.s3.api...`;
- `ua.homeguard.s3.data...`;
- `ua.homeguard.s3.ui.main.MainUiState` / `MainViewModel`.

PR #51 removed exactly 8 source files / 203 lines from the first three families, but its Android CI failed at compile time because `ui/main/MainUiState.kt` still imports `DeviceStateDto` and `ui/main/MainViewModel.kt` still imports `CommandResponseDto` and `HomeGuardRepository`. Therefore PR #51 is proof of candidate dead code, **not** a safe cherry-pick/delete set. Repository search found no external references to `MainViewModel`, `MainUiState`, `AlarmAcknowledgementController`, or `HomeGuardRepository`, but cleanup still requires a complete source/test call-graph check before deletion.

Action: identify the full obsolete island (including tests), then remove it atomically in one isolated minimal commit and require Android CI green.

#### A-021 — RegisteredDeviceStore global `activeStore` bridge is architectural debt — OPEN
The process-global bridge is tolerated on the audit branch for narrow fixes and is now also used to close the provisioning shortcut naming bypass. Do not expand it further. Replace it with explicit registry injection when A-018/A-004 are addressed.

## Confirmed positives
- API and telemetry tokens use `SecureTokenStore` / Android Keystore AES-GCM.
- First save in `RegisteredDeviceStore.addOrUpdate()` requires an owner-provided friendly name.
- Device List supports rename/delete/properties, red unauthorized state, single-tap quick state and double-tap full monitoring.
- Factory Reset client requires explicit `ERASE_ALL`.
- Build #1115 passed on audit head `74665b6933daa44f412669a4445afffa191b8ddb` before the latest provisioning-shortcut code changes.

## Current execution status
Verified/fixed on the audit branch so far:
- credential storage and legacy migration;
- discovery deduplication, progress, stale HTTP clearing and aggregate TTL;
- compact Bruce and Device List picon/UNKNOWN/narrow-layout behavior;
- stale telemetry isolation;
- Factory Reset transport classification;
- device-bound secret policy;
- selected/provisioned ID normalization;
- QR owner-friendly-name registry path;
- already-connected/manual provisioning shortcut friendly-name registry gate + policy test;
- legacy unnamed-device recovery;
- secondary Add entry;
- cancellation-safe OkHttp bridge;
- cancellation-safe Setup AP handoff.

Latest code/test head before this register commit: `0190c7bdea0feea2571171dca9cd1c07cd4651ab`. Build #1118 was queued/pending when this register was updated; do not treat the latest shortcut changes as CI-validated until that run succeeds.

## Next immediate blocks
1. Re-check Build #1118 and fix any exact failure before more code changes.
2. Remove the duplicate `ProvisioningScreen` discovery/settings runtime and `Activity.recreate()` dependency (A-018) using existing MainActivity-owned state/actions.
3. Continue lifecycle/coroutine review around NSD, network requests, telemetry and Activity transitions.
4. Complete the obsolete `ui/main` + API/data/alarm call graph and remove only when the full island is proven dead.
5. Plan MainActivity decomposition after contracts are covered.
6. Audit manifest/build/security surface, especially global cleartext and permission timing.
7. Strengthen acceptance gates for the fixed Device List/provisioning rules.

## Audit rule
Do not merge to `main` during discovery. Keep changes isolated on the audit branch, run CI continuously, and split/merge only verified minimal changes after the register is complete and phone validation passes.
