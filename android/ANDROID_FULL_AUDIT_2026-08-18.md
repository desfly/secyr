# HomeGuard-S3 Android full audit — 2026-08-18

Branch: `audit/android-full-20260818`
Base: `main`
Draft PR: `#53`

## Goal
Perform a complete Android audit before broad refactoring. Classify findings as `CRITICAL`, `HIGH`, `MEDIUM`, or `CLEANUP`; preserve the cemented device-list/discovery/auth/factory-reset contract; avoid blind merges from older PRs.

## Day plan
- 07:00–08:00 — repository inventory, startup/navigation, manifest/build/security baseline.
- 08:00–09:00 — discovery stack: mDNS/UDP/HTTP/manual IP, deduplication and controller identity.
- 09:00–10:00 — registered-device persistence, friendly names, selection, rename/delete/revoked access.
- 10:00–11:00 — Device List / Add Device UI and navigation contract.
- 11:00–12:00 — authentication/session/credential storage/security.
- 12:00–13:00 — provisioning, Wi-Fi permissions and first-run flows.
- 13:00–14:00 — telemetry, command path, endpoint resolver and offline/cloud behavior.
- 14:00–15:00 — Factory Reset, backup/restore and destructive-action handling.
- 15:00–16:00 — lifecycle/coroutines/background/foreground/crash-risk audit.
- 16:00–17:00 — unit/integration/UI test coverage audit and missing acceptance gates.
- 17:00–18:00 — duplicate/dead architecture review, including old API/data layers and PR #51 context.
- 18:00–19:00 — build configuration, dependencies, manifest, release/debug packaging and security surface.
- 19:00–20:00 — consolidate findings, prioritize fixes, prepare first minimal change set.

## Findings register

### CRITICAL

#### A-001 — Plaintext first-run password in SharedPreferences — FIXED ON AUDIT BRANCH
Files:
- `android/app/src/main/java/ua/homeguard/s3/ui/screens/DeviceListScreen.kt`
- `android/app/src/main/java/ua/homeguard/s3/storage/LocalProfileStore.kt`

Previous behavior wrote the first-run profile password directly to `myfist_profile` SharedPreferences.

Fix: password now goes through `SecureTokenStore` (Android Keystore + AES/GCM). `LocalProfileStore` also migrates/removes the legacy plaintext `password` key.

### HIGH

#### A-002 — Discovery deduplication was host-grouped, not transitive identity reconciliation — FIXED
Files:
- `android/app/src/main/java/ua/homeguard/s3/network/DiscoveryDeduplicator.kt`
- `android/app/src/main/java/ua/homeguard/s3/network/LocalDiscoveryCoordinator.kt`
- `android/app/src/test/java/ua/homeguard/s3/network/DiscoveryDeduplicatorTest.kt`

Fix: union-find transitive reconciliation using `ControllerIdentity.sameController()`. Stable `HG-*` identity is preserved while endpoint details can come from the freshest discovery report. Tests cover same ID/different host, setup fallback/stable ID, transitive merge, unrelated controllers, and source priority.

#### A-003 — Oversized/duplicated Bruce in Device List — FIXED
Files:
- `android/app/src/main/java/ua/homeguard/s3/ui/screens/DeviceListScreen.kt`
- `android/app/src/main/java/ua/homeguard/s3/ui/components/BruceBrand.kt`

Fix: removed the separate full-width square Bruce image from the normal Device List and restored a compact 44dp Bruce header mark.

#### A-007 — Stale WebSocket frames could overwrite the newly selected device — FIXED
File: `android/app/src/main/java/ua/homeguard/s3/network/TelemetrySocket.kt`

Fix: stale socket callbacks now return before parsing or publishing any telemetry.

#### A-008 — Any Factory Reset IOException was treated as expected reboot/disconnect — FIXED
Files:
- `android/app/src/main/java/ua/homeguard/s3/network/FactoryResetClient.kt`
- `android/app/src/test/java/ua/homeguard/s3/network/FactoryResetTransportPolicyTest.kt`

Fix: OkHttp `requestBodyEnd` tracks whether the destructive request body was actually sent. DNS/connect/TLS failures before submission are no longer treated as likely successful reset disconnects.

#### A-009 — Settings restore could erase telemetry token / device switches could reuse another controller's secret — FIXED
Files:
- `android/app/src/main/java/ua/homeguard/s3/storage/SettingsStore.kt`
- `android/app/src/test/java/ua/homeguard/s3/storage/SettingsSecretPolicyTest.kt`

Fix: secrets are now device-bound. Same-device restore preserves omitted local secrets; normal device selection clears carried-over API/telemetry tokens; provisioning may still install a genuinely new token while changing devices. This also clears stale device-bound secrets when the selected controller is cleared after Factory Reset.

#### A-010 — Prominent Add Device action conflicts with the cemented hidden/secondary Add flow — FIXED ON AUDIT BRANCH
File: `android/app/src/main/java/ua/homeguard/s3/ui/screens/DeviceListScreen.kt`

Fix: the full-width primary `+ Додати пристрій` button was removed from the normal device-list header and replaced with a small secondary `TextButton`. The Add screen itself is not redesigned and Android still starts on the device list.

#### A-011 — Manual rescan progress reflected UDP only, not UDP+HTTP — FIXED
Files:
- `android/app/src/main/java/ua/homeguard/s3/network/HttpSubnetDiscovery.kt`
- `android/app/src/main/java/ua/homeguard/s3/network/LocalDiscoveryCoordinator.kt`
- `android/app/src/test/java/ua/homeguard/s3/network/DiscoveryScanStatusTest.kt`

Fix: HTTP subnet scanning now exposes actual probe progress; coordinator-level status combines UDP+HTTP and keeps manual search active until both branches finish. Tests cover partial HTTP progress, final 100%, errors, and background UDP scans.

#### A-014 — HTTP discovery could leave stale devices visible after Wi-Fi disappeared — FIXED
File: `android/app/src/main/java/ua/homeguard/s3/network/HttpSubnetDiscovery.kt`

Fix: missing Wi-Fi network/IPv4 now clears stale HTTP discovery results and reports a concrete scan error.

#### A-015 — Discovery UI displayed duplicate “Знайдено” counters — FIXED
File: `android/app/src/main/java/ua/homeguard/s3/ui/screens/AddDeviceScreen.kt`

Fix: the scan-status panel now reports only search state/completion; the actual result count is rendered once with the result list.

#### A-016 — Provisioning handoff compared controller IDs case-sensitively — FIXED
Files:
- `android/app/src/main/java/ua/homeguard/s3/provisioning/ProvisioningHandoff.kt`
- `android/pure-tests/ProvisioningHandoffTest.kt`
- `android/app/src/main/java/ua/homeguard/s3/repository/ProvisioningCoordinator.kt`

Fix: QR/handoff and post-reboot discovery now trim and compare stable controller IDs case-insensitively. HTTPS scheme validation is also case-insensitive. The pure handoff test now proves a lower-case discovered ID still matches the QR identity.

#### A-017 — Provisioning could bypass the owner-friendly-name registry contract — FIXED FOR QR PROVISIONING
Files:
- `android/app/src/main/java/ua/homeguard/s3/repository/ProvisioningCoordinator.kt`
- `android/app/src/main/java/ua/homeguard/s3/storage/RegisteredDeviceStore.kt`
- `android/app/src/main/java/ua/homeguard/s3/ui/screens/ProvisioningScreen.kt`

Previous flow accepted `ownerLabel` in the UI but did not require it and did not register the QR-provisioned controller in `RegisteredDeviceStore`.

Fix: QR provisioning rejects a blank owner label, caps the UI value at 40 characters, disables the provision button until the owner enters a name, registers the stable QR device ID with that owner-provided name, and refreshes the registry entry with the discovered LAN endpoint after reboot.

#### A-018 — ProvisioningScreen creates a second discovery/settings runtime inside Compose — OPEN
File: `android/app/src/main/java/ua/homeguard/s3/ui/screens/ProvisioningScreen.kt`

The top-level `ProvisioningScreen` constructs another `LocalDiscoveryCoordinator` and another `SettingsStore` even though `MainActivity` already owns the application discovery/settings runtime. This duplicates network work and forces a full `Activity.recreate()` after local/manual selection just to make the primary `SettingsStore` observe the secondary store's write.

Action: pass the existing runtime state/actions into the screen and remove the screen-owned discovery/settings instances.

#### A-019 — HttpDeviceApi cancellation could race with OkHttp response delivery and leak Response — FIXED ON AUDIT BRANCH
File: `android/app/src/main/java/ua/homeguard/s3/network/HttpDeviceApi.kt`

Fix: coroutine cancellation cancels the OkHttp call and public cancellable-continuation `resume(response) { ... }` closes a response that cannot be delivered to the caller. This avoids internal kotlinx.coroutines APIs and keeps the caller's `.use` ownership for successfully delivered responses.

#### A-022 — Stale mDNS reports could keep a disappeared controller online indefinitely — FIXED ON AUDIT BRANCH
Files:
- `android/app/src/main/java/ua/homeguard/s3/network/LocalDiscoveryCoordinator.kt`
- `android/app/src/test/java/ua/homeguard/s3/network/DiscoveryScanStatusTest.kt`

`NsdDeviceDiscovery` only pruned its cache when another mDNS publish event occurred. If Android omitted `onServiceLost`, an old mDNS entry could remain in the combined device list indefinitely even though its `seenAtMs` was older than the intended 30-second lifetime.

Fix: the coordinator filters all source reports by the common 30-second freshness window before transitive deduplication. The periodic UDP flow re-evaluates the combined list, so stale mDNS entries are removed even without another NSD callback. A boundary test covers fresh-vs-stale reports.

#### A-023 — Provisioning “already on Wi-Fi” and manual-IP shortcuts still bypass the registry name contract — OPEN
File: `android/app/src/main/java/ua/homeguard/s3/ui/screens/ProvisioningScreen.kt`

The secondary provisioning screen still calls a screen-owned `SettingsStore.remember()` / `selectDevice()` for already-connected or manual-IP devices without first writing an owner-named `RegisteredDevice`. This can create a selected controller that has no normal device-list card/name.

Action: eliminate the duplicate screen runtime (A-018) and route these shortcuts through the same application-level registry methods used by Add Device, requiring the owner name before selection.

#### A-024 — Setup AP cancellation could leave the whole process bound to the temporary Wi-Fi network — FIXED ON AUDIT BRANCH
File: `android/app/src/main/java/ua/homeguard/s3/provisioning/SetupNetworkConnector.kt`

Previous `onAvailable()` bound the process to the Setup AP and then resumed the coroutine after an `isActive` check. Cancellation could win between the check and resume, leaving the result unclaimed while the process remained bound to the temporary network. Repeated terminal callbacks could also race.

Fix: terminal callback delivery is guarded by `AtomicBoolean`; cancelled/unclaimed `BoundSetupNetwork` values close themselves through the cancellable `resume` callback; cancellation before delivery unregisters the network callback; bind failures close/unregister before propagating the exception.

### MEDIUM

#### A-004 — MainActivity is an orchestration god-object — OPEN
File: `android/app/src/main/java/ua/homeguard/s3/MainActivity.kt`

`MainActivity` owns discovery, settings, registry, event history, endpoint resolution, provisioning, telemetry, session, commands, notifications, navigation state, operator state, backup/restore launchers, QR flow, factory reset, and top-level Compose routing.

Action: after behavior contracts are covered, split app/navigation state from networking/storage/runtime services.

#### A-005 — Global cleartext HTTP is enabled for the entire app — OPEN
File: `android/app/src/main/AndroidManifest.xml`

`android:usesCleartextTraffic="true"` applies globally. Local ESP access may require HTTP during setup/bench use, but the current setting broadens the cleartext surface to every destination.

Action: audit endpoints and constrain cleartext policy without breaking local controller discovery/setup.

#### A-012 — Selected controller ID comparison was case-sensitive — FIXED
File: `android/app/src/main/java/ua/homeguard/s3/network/DeviceEndpointResolver.kt`

Direct local selection now compares stable device IDs case-insensitively.

#### A-013 — Legacy registry entries could synthesize visible name `HomeGuard` — FIXED ON AUDIT BRANCH
Files:
- `android/app/src/main/java/ua/homeguard/s3/storage/RegisteredDeviceStore.kt`
- `android/app/src/main/java/ua/homeguard/s3/ui/screens/DeviceListScreen.kt`

Fix: a missing legacy `name` no longer defaults to `HomeGuard`; it remains unnamed in storage. The device card shows a migration prompt `Потрібна назва`, and single/double tap routes directly to rename until the owner provides a real friendly name. No generated HomeGuard/ID/IP name is exposed as the normal card title.

#### A-020 — Device-state picons conflated authorization with armed state and treated UNKNOWN as a red fault — FIXED ON AUDIT BRANCH
File: `android/app/src/main/java/ua/homeguard/s3/ui/screens/DeviceListScreen.kt`

Fix: authorization now renders as `🛡 доступ` instead of the misleading `🛡 знято`; missing/UNKNOWN telemetry is neutral (`… стан`), actual fault/alarm-like health remains red, and all compact labels are single-line weighted cells with ellipsis on narrow screens.

### CLEANUP

#### A-006 — Two parallel/dead Android architecture families exist — OPEN
Paths include:
- `ua.homeguard.app.alarm...`
- `ua.homeguard.s3.api...`
- `ua.homeguard.s3.data...`

PR #51 proves a first candidate set for removal, but its old standalone alarm mapping test also references the obsolete layer. Cleanup must include tests/call graph, not blind file deletion.

#### A-021 — RegisteredDeviceStore global activeStore bridge is architectural debt — OPEN
File: `android/app/src/main/java/ua/homeguard/s3/storage/RegisteredDeviceStore.kt`

The existing static `activeStore` bridge is now also used by the minimal provisioning contract fix. It is acceptable as an isolated audit-branch bridge, but final cleanup should inject the registry into the application runtime/coordinator instead of relying on a process-global active instance.

## Confirmed positives
- API and telemetry tokens are protected through `SecureTokenStore`.
- `SecureTokenStore` uses Android Keystore with AES/GCM.
- `RegisteredDeviceStore.addOrUpdate()` refuses first save without a nonblank owner-provided friendly name.
- `DeviceListScreen` supports rename, delete, properties, red unauthorized state, single-tap expansion and double-tap opening.
- Factory Reset request already requires explicit `ERASE_ALL` at the API client layer.
- Build #1109 passed after correcting the prior audit-branch compile regressions; later audit commits must still pass their own CI before they are considered validated.

## Current execution status
Completed in the active audit branch:
- repository/navigation/security baseline;
- first-run credential storage hardening + legacy migration;
- discovery deduplication redesign + tests;
- real UDP+HTTP search progress + stale HTTP result clearing + tests;
- aggregate 30-second stale discovery expiry + test;
- duplicate result counter removal;
- compact Bruce header / oversized Bruce removal;
- stale telemetry isolation;
- Factory Reset transport classification + tests;
- selected-ID normalization;
- device-bound API/telemetry secret policy + tests;
- provisioning ID normalization + test;
- QR provisioning owner-friendly-name registration + UI gating;
- legacy unnamed-device recovery prompt;
- Add Device demoted from a primary full-width action to a secondary entry;
- Device List picon/UNKNOWN/narrow-layout correction;
- cancellation-safe OkHttp coroutine bridge;
- cancellation-safe Setup AP network binding handoff.

Next immediate blocks:
1. validate the latest owner-name/discovery/setup-network changes in CI and fix any exact failure first;
2. remove the duplicate `ProvisioningScreen` discovery/settings runtime and close the already-connected/manual registry bypass;
3. continue lifecycle/coroutine review for NSD, network requests, telemetry and Activity transitions;
4. MainActivity decomposition plan;
5. dead/duplicate architecture and PR #51 comparison;
6. manifest/build/security-surface review, especially global cleartext and permission timing;
7. add/strengthen acceptance gates for the newly fixed Device List and provisioning rules.

## Audit rule
Do not merge to `main` during discovery. Keep changes isolated on the audit branch, run CI continuously, and only later split/merge verified minimal changes.
