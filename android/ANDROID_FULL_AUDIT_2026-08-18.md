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

#### A-001 — Plaintext first-run password in SharedPreferences
File: `android/app/src/main/java/ua/homeguard/s3/ui/screens/DeviceListScreen.kt`

`FirstRunRegistrationScreen` writes the entered password directly to `myfist_profile` SharedPreferences via `putString("password", password)`. The project already has `SecureTokenStore` backed by Android Keystore/AES-GCM, so plaintext credential persistence is unnecessary and inconsistent with the secure-token path.

Impact: local credential exposure through app data/debug/device compromise; security architecture inconsistency.

Action: determine whether this local profile password is actually required. If required, move it to secure storage; if it is only a UI placeholder, remove password persistence entirely.

### HIGH

#### A-002 — Discovery deduplication is keyed primarily by host, not stable controller identity
File: `android/app/src/main/java/ua/homeguard/s3/network/LocalDiscoveryCoordinator.kt`

The merged mDNS/UDP/HTTP discovery list uses `physicalControllerKey()` which returns `host:<host>` whenever host is available and only falls back to `deviceId`. This can leave duplicate UI devices when one controller is reported under different host representations or after an address change, even though `ControllerIdentity.sameController()` already supports ID-first equivalence.

Impact: violates the cemented rule “one physical ESP = one UI card”.

Action: replace one-shot `groupBy(host)` with transitive reconciliation based on `ControllerIdentity.sameController()` (stable ID OR normalized endpoint/host), then add regression tests for cross-transport and IP-change cases.

#### A-003 — Oversized Bruce artwork is still present in the Device List flow
File: `android/app/src/main/java/ua/homeguard/s3/ui/screens/DeviceListScreen.kt`

The screen adds a full-width square `SafeBruceImage` (`fillMaxWidth().aspectRatio(1f)`) after the header. This contradicts the compact-header requirement and matches the previously observed oversized-Bruce regression.

Impact: consumes a large part of the first viewport and degrades the primary device-list UX.

Action: retain a compact brand/icon in the header and remove the separate full-width square artwork from the normal device-list screen.

### MEDIUM

#### A-004 — MainActivity is an orchestration god-object
File: `android/app/src/main/java/ua/homeguard/s3/MainActivity.kt`

`MainActivity` owns discovery, settings, registry, event history, endpoint resolution, provisioning, telemetry, session, commands, notifications, navigation state, operator state, backup/restore launchers, QR flow, factory reset, and the entire top-level Compose routing.

Impact: difficult lifecycle reasoning, high regression risk, poor test isolation.

Action: after audit, split app/navigation state from networking/storage/runtime services. Do not refactor until behavior contracts are covered by tests.

#### A-005 — Global cleartext HTTP is enabled for the entire app
File: `android/app/src/main/AndroidManifest.xml`

`android:usesCleartextTraffic="true"` applies globally. Local ESP access may require HTTP during development, but the current setting broadens the cleartext surface to all destinations.

Action: audit all endpoints and, if local HTTP remains required, constrain policy with Network Security Config instead of a global allow.

### CLEANUP

#### A-006 — Two parallel package families exist in the Android source tree
Paths: `ua.homeguard.app...` and `ua.homeguard.s3...`

The tree contains both the current `ua.homeguard.s3` runtime and an older/parallel `ua.homeguard.app` layer. PR #51 already targets part of this cleanup.

Action: establish call/reference graph and remove only proven-unused code after tests cover the active path.

## Confirmed positives
- `SettingsStore` already stores API and telemetry tokens through `SecureTokenStore` rather than plaintext preferences.
- `SecureTokenStore` uses Android Keystore with AES/GCM.
- `RegisteredDeviceStore.addOrUpdate()` refuses first save without a nonblank owner-provided friendly name.
- `DeviceListScreen` supports rename, delete, properties, red unauthorized state, single-tap expansion and double-tap opening.

## Audit rule
Do not merge PRs or perform broad refactors during discovery. First finish the findings register, then add acceptance tests, then make small verified changes.
