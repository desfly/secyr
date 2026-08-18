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

#### A-010 — Prominent Add Device action conflicts with the cemented hidden/secondary Add flow — OPEN
File: `android/app/src/main/java/ua/homeguard/s3/ui/screens/DeviceListScreen.kt`

The normal device list still exposes a full-width `+ Додати пристрій` button. The product rule says the current Add flow remains hidden/secondary until its structure is redesigned separately.

Action: replace the primary action with the smallest secondary entry point and lock it with a UI/source contract test; do not redesign Add Device.

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

#### A-017 — Provisioning could bypass the owner-friendly-name registry contract — FIXED AT COORDINATOR LEVEL
Files:
- `android/app/src/main/java/ua/homeguard/s3/repository/ProvisioningCoordinator.kt`
- `android/app/src/main/java/ua/homeguard/s3/storage/RegisteredDeviceStore.kt`

Previous flow accepted `ownerLabel` in the UI but did not require it and did not register the QR-provisioned controller in `RegisteredDeviceStore`. A successfully provisioned controller could therefore be selected in settings without a proper owner-named device card.

Fix: provisioning now rejects a blank owner label, registers the stable QR device ID with the owner-provided name after the controller accepts configuration, and refreshes that registry entry with the discovered LAN endpoint after reboot. The UI button still needs a small follow-up so it is disabled before the name is entered instead of surfacing the validation error after tap.

#### A-018 — ProvisioningScreen creates a second discovery/settings runtime inside Compose — OPEN
File: `android/app/src/main/java/ua/homeguard/s3/ui/screens/ProvisioningScreen.kt`

The top-level `ProvisioningScreen` constructs another `LocalDiscoveryCoordinator` and another `SettingsStore` even though `MainActivity` already owns the application discovery/settings runtime. This duplicates network work and can produce competing state/lifecycle behavior.

Action: after behavior contracts are locked, pass the existing runtime state/actions into the screen and remove the screen-owned discovery/settings instances.

#### A-019 — HttpDeviceApi cancellation can race with OkHttp response delivery and leak Response — OPEN
File: `android/app/src/main/java/ua/homeguard/s3/network/HttpDeviceApi.kt`

`Call.await()` cancels OkHttp when the coroutine is cancelled, but `onResponse()` unconditionally resumes with the `Response`. If cancellation wins between network completion and resume, the response may no longer reach the caller's `.use` block and can be leaked; `onFailure()` also uses an `isActive` check rather than atomic `tryResume*` handling.

Action: use atomic cancellable-continuation resume (`tryResume` / `tryResumeWithException`) and explicitly close an unclaimed response.

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

#### A-013 — Legacy registry entries could synthesize visible name `HomeGuard` — PARTIAL FIX
File: `android/app/src/main/java/ua/homeguard/s3/storage/RegisteredDeviceStore.kt`

Fix applied: a missing legacy `name` no longer defaults to `HomeGuard`; it remains blank, preserving the invariant that the storage layer does not invent a normal-card name.

Remaining UI migration: an unnamed legacy entry needs an explicit “name required” prompt/rename path so the device remains recoverable without showing a generated product/ID/IP name.

#### A-020 — Device-state picons conflate authorization with armed state and treat UNKNOWN as a red fault — OPEN
File: `android/app/src/main/java/ua/homeguard/s3/ui/screens/DeviceListScreen.kt`

Current `authorized=true` renders `🛡 знято`, although authorization is not the security arm/disarm state. The health picon also renders `⚠ UNKNOWN` before a valid snapshot and three long labels share one non-wrapping Row, reproducing the narrow-screen health-layout problem.

Action: make authorization wording explicit, render unknown/no-snapshot neutrally, classify actual fault/alarm states separately, and prevent label wrapping/overflow.

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

## Current execution status
Completed in the active audit branch:
- repository/navigation/security baseline;
- first-run credential storage hardening + legacy migration;
- discovery deduplication redesign + tests;
- real UDP+HTTP search progress + stale HTTP result clearing + tests;
- duplicate result counter removal;
- compact Bruce header / oversized Bruce removal;
- stale telemetry isolation;
- Factory Reset transport classification + tests;
- selected-ID normalization;
- device-bound API/telemetry secret policy + tests;
- provisioning ID normalization + test;
- provisioning owner-friendly-name registration path;
- removal of the `HomeGuard` fallback for missing legacy names.

Next immediate blocks:
1. run CI for the latest provisioning/registry changes and fix any exact failure first;
2. Add Device hidden/secondary entry contract;
3. legacy unnamed-card rename prompt;
4. Device List picon/health layout regression;
5. HttpDeviceApi cancellation safety;
6. lifecycle/coroutine review, including duplicate ProvisioningScreen runtime;
7. MainActivity decomposition plan;
8. dead/duplicate architecture and PR #51 comparison;
9. manifest/build/security-surface review.

## Audit rule
Do not merge to `main` during discovery. Keep changes isolated on the audit branch, run CI continuously, and only later split/merge verified minimal changes.
