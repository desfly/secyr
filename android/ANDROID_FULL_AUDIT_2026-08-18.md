# HomeGuard-S3 Android full audit — 2026-08-18

Branch: `audit/android-full-20260818`
Base: `main`

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

#### A-002 — Discovery deduplication was host-grouped, not transitive identity reconciliation — FIXED ON AUDIT BRANCH
Files:
- `android/app/src/main/java/ua/homeguard/s3/network/DiscoveryDeduplicator.kt`
- `android/app/src/main/java/ua/homeguard/s3/network/LocalDiscoveryCoordinator.kt`
- `android/app/src/test/java/ua/homeguard/s3/network/DiscoveryDeduplicatorTest.kt`

Previous `groupBy(host)` could leave duplicates when mDNS/UDP/HTTP observed one ESP through different hosts/IDs.

Fix: union-find transitive reconciliation using `ControllerIdentity.sameController()`. Stable `HG-*` identity is preserved while endpoint details can come from the freshest discovery report. Tests cover same ID/different host, setup fallback/stable ID, transitive merge, unrelated controllers, and source priority.

#### A-003 — Oversized Bruce artwork in Device List — FIXED ON AUDIT BRANCH
File: `android/app/src/main/java/ua/homeguard/s3/ui/screens/DeviceListScreen.kt`

The separate full-width square Bruce image was removed from the normal device-list flow. Compact `BruceBrand` remains in the header.

#### A-007 — Stale WebSocket frames could overwrite the newly selected device — FIXED ON AUDIT BRANCH
File: `android/app/src/main/java/ua/homeguard/s3/network/TelemetrySocket.kt`

`onMessage()` checked whether the callback belonged to the active socket only before setting connection state, but still parsed and published the message when the socket was stale. During asynchronous close/reconnect, telemetry from the previous controller could therefore overwrite the current snapshot/event list.

Fix: stale socket callbacks now return before parsing or publishing any telemetry.

#### A-008 — Any Factory Reset IOException was treated as expected reboot/disconnect — FIXED ON AUDIT BRANCH
Files:
- `android/app/src/main/java/ua/homeguard/s3/network/FactoryResetClient.kt`
- `android/app/src/test/java/ua/homeguard/s3/network/FactoryResetTransportPolicyTest.kt`

DNS/connect/TLS failures before the destructive request was submitted were previously returned as `CONNECTION_LOST`, causing Android to clear its local selected-device/session state as if reset had likely been accepted.

Fix: OkHttp `EventListener.requestBodyEnd` tracks whether the request body was actually sent. Only a later transport loss becomes `CONNECTION_LOST`; failures before submission are `REJECTED`. Policy is unit-tested.

#### A-009 — Settings restore can erase the dedicated telemetry token — OPEN
Files:
- `android/app/src/main/java/ua/homeguard/s3/storage/SettingsBackupCodec.kt`
- `android/app/src/main/java/ua/homeguard/s3/MainActivity.kt`

`SettingsBackupCodec.decode(text, currentToken)` preserves `apiToken` only. `telemetryToken` falls back to the `AppSettings` default (`""`) and `SettingsStore.update()` then removes the stored secure telemetry token. This can break the authenticated telemetry socket after restore.

Action: preserve both local secrets across import while continuing to exclude them from exported JSON. Add regression test before changing behavior.

#### A-010 — Prominent Add Device action conflicts with the cemented hidden/secondary Add flow — OPEN
File: `android/app/src/main/java/ua/homeguard/s3/ui/screens/DeviceListScreen.kt`

The normal device list still exposes a full-width `+ Додати пристрій` button. The cemented product rule says the current Add flow remains hidden/secondary until its structure is redesigned separately.

Action: define the smallest secondary entry point without redesigning the Add screen, then lock it with UI contract tests.

#### A-011 — Manual rescan completion/progress currently reflects UDP, not the whole UDP+HTTP scan — OPEN
Files:
- `android/app/src/main/java/ua/homeguard/s3/network/LocalDiscoveryCoordinator.kt`
- `android/app/src/main/java/ua/homeguard/s3/network/HttpSubnetDiscovery.kt`

`rescan()` awaits UDP and HTTP scans, but `isScanning` and `scanStatus` are derived only from UDP status. The UI can therefore report completion while HTTP subnet probing is still running.

Action: expose real coordinator-level scan state/progress covering both discovery branches.

### MEDIUM

#### A-004 — MainActivity is an orchestration god-object — OPEN
File: `android/app/src/main/java/ua/homeguard/s3/MainActivity.kt`

`MainActivity` owns discovery, settings, registry, event history, endpoint resolution, provisioning, telemetry, session, commands, notifications, navigation state, operator state, backup/restore launchers, QR flow, factory reset, and top-level Compose routing.

Impact: difficult lifecycle reasoning, high regression risk, poor test isolation.

Action: after behavior contracts are covered, split app/navigation state from networking/storage/runtime services.

#### A-005 — Global cleartext HTTP is enabled for the entire app — OPEN
File: `android/app/src/main/AndroidManifest.xml`

`android:usesCleartextTraffic="true"` applies globally. Local ESP access may require HTTP during setup/bench use, but the current setting broadens the cleartext surface to every destination.

Action: audit endpoints and constrain cleartext policy without breaking local controller discovery/setup.

#### A-012 — Selected controller ID comparison was case-sensitive — FIXED ON AUDIT BRANCH
File: `android/app/src/main/java/ua/homeguard/s3/network/DeviceEndpointResolver.kt`

Direct local selection now compares stable device IDs case-insensitively, matching the identity semantics already used by the registry.

#### A-013 — Legacy registry entries can synthesize the visible name `HomeGuard` — OPEN
File: `android/app/src/main/java/ua/homeguard/s3/storage/RegisteredDeviceStore.kt`

`load()` defaults a missing persisted name to `HomeGuard`, conflicting with the rule that normal cards show only the owner-assigned friendly name.

Action: migrate or quarantine unnamed legacy entries rather than generating a product name in the normal list.

### CLEANUP

#### A-006 — Two parallel package families exist in the Android source tree — OPEN
Paths: `ua.homeguard.app...` and `ua.homeguard.s3...`

The tree contains both the current `ua.homeguard.s3` runtime and an older/parallel `ua.homeguard.app` layer. PR #51 already targets part of this cleanup.

Action: establish call/reference graph and remove only proven-unused code after tests cover the active path.

## Confirmed positives
- `SettingsStore` stores API and telemetry tokens through `SecureTokenStore` rather than plaintext preferences.
- `SecureTokenStore` uses Android Keystore with AES/GCM.
- `RegisteredDeviceStore.addOrUpdate()` refuses first save without a nonblank owner-provided friendly name.
- `DeviceListScreen` supports rename, delete, properties, red unauthorized state, single-tap expansion and double-tap opening.
- Factory Reset request already requires explicit `ERASE_ALL` at the API client layer.

## Current execution status
Completed in the first audit pass:
- repository/navigation/security baseline;
- first-run credential storage fix;
- discovery deduplication redesign + tests;
- oversized Device List Bruce regression removal;
- stale telemetry isolation fix;
- Factory Reset transport classification fix + tests;
- selected-ID case normalization.

Next immediate blocks:
1. backup/restore secret preservation;
2. real full-scan progress semantics;
3. registry legacy-name migration;
4. Add Device visibility contract;
5. lifecycle/coroutine review;
6. dead/duplicate architecture and PR #51 comparison.

## Audit rule
Do not merge to `main` during discovery. Keep changes isolated on the audit branch, run CI continuously, and only later split/merge verified minimal changes.
