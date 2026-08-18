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
- **A-030 — Discovery refresh caused repeated persistent registry writes — FIXED + CI VALIDATED.** Timestamp-only registry persistence is throttled to 60 s while ID/base-URL changes persist immediately. Original 5-second cadence meant up to 12 timestamp writes/minute; now at most 1/minute per continuously discovered controller (~12× reduction). Build #1172 passed.
- **A-031 — Duplicate telemetry sequence could trigger duplicate history write/notification — FIXED + CI VALIDATED.** Duplicate event identity is suppressed before history/notification fan-out. Build #1172 passed; later multi-controller hardening makes identity controller-aware.
- **A-032 — No-op settings/registry mutations wrote SharedPreferences/secure storage and emitted Flow state — FIXED + CI VALIDATED.** Stores return early when normalized state/list is unchanged. Build #1172 passed.
- **A-033 — HTTP subnet discovery accepted unbounded 200-OK bodies — FIXED + CI VALIDATED.** Discovery JSON is capped at 64 KiB characters. Build #1172 passed.
- **A-034 — DeviceEndpointResolver duplicated registry/settings reconciliation side effects — FIXED + CI VALIDATED.** Resolver is read-only; one MainActivity-owned reconciliation collector remains. Build #1172 passed.
- **A-035 — Event history reparsed all persisted JSON on each event — FIXED + CI VALIDATED.** History loads once into synchronized memory cache; duplicate identity is zero-write. Build #1172 passed.
- **A-036 — Parallel HTTP scan progress could regress due to out-of-order worker updates — FIXED + CI VALIDATED.** Only the short progress update is serialized; 32 HTTP probes remain parallel. Build #1172 passed.
- **A-037 — Case-only device-ID change could clear route/certificate or survive deletion as selected — FIXED.** Device selection/delete matching is case-insensitive; test covers `HG-AbC123` → `hg-abc123`.
- **A-038 — Wi-Fi scan body cap missing for Content-Length responses — FIXED.** Both body paths are capped at 256,000 bytes.
- **A-039 — Discovery work continued while Activity was backgrounded — FIXED.** Periodic mDNS/UDP/freshness work now follows `onStart()`/`onStop()` instead of running until `onDestroy()`. Telemetry/session intentionally remains alive for monitoring/notifications.
- **A-040 — Old NSD DiscoveryListener callback could mutate a newer discovery generation — FIXED.** Every start owns a distinct listener; `found`, `lost`, and `resolve` callbacks are generation-gated. Old callbacks cannot mutate a newer discovery run.
- **A-041 — Repeated provisioning tap could run two Setup-AP/token/settings flows — FIXED.** Provisioning is single-flight via an atomic gate released in `finally`.
- **A-042 — Stale login could attach an old controller telemetry token/session after device switch — FIXED.** Login re-validates selected controller after login response, after telemetry-session response, and before returning.
- **A-043 — Event sequence identity collided across different controllers — FIXED.** Event dedup/history/notifications carry controller identity, so `sequence=42` from HG-A and HG-B are separate while same-controller duplicates remain suppressed.
- **A-045 — Dashboard mixed histories from multiple controllers and reused Compose event keys — FIXED + ANDROID/HOST CI VALIDATED.** Dashboard now renders selected-controller events plus legacy unscoped records only. Event list keys use `controllerId + sequence`, preventing equal sequence numbers from two ESPs colliding in Compose. Focused unit test covers filtering and key identity. Build #1213 Android + host jobs passed on the exact code head.
- **A-046 — Operator session and Factory Reset completion were not bound to the controller that issued them — FIXED + ANDROID/HOST CI VALIDATED.** `AccessSession` now carries `controllerId`; device selection changes clear a stale operator session/PIN; commands and Factory Reset require the session to belong to the selected controller. Factory Reset captures its target controller and will not clear a different controller selected while the destructive request is in flight. Build #1213 Android + host jobs passed on the exact code head.
- **A-047 — Live telemetry fan-out could silently drop bursts beyond 16 queued events — FIXED + ANDROID/HOST CI VALIDATED.** The unchecked `MutableSharedFlow(extraBufferCapacity=16).tryEmit()` path was replaced by the single-consumer event queue used by persistent history + notifications. Accepted unique events are no longer silently dropped because the old 16-event intermediate buffer filled. Build #1213 Android + host jobs passed on the exact code head.
- **A-048 — WebSocket callback could race assignment of the active socket — FIXED + ANDROID/HOST CI VALIDATED.** Connection callbacks are gated by a monotonic generation token created before `newWebSocket()`. Stale generations are rejected and disconnect invalidates the previous generation, removing dependence on callback-vs-assignment timing. Unit test covers current/stale generation acceptance. Build #1213 Android + host jobs passed on the exact code head.
- **A-050 — In-flight command/login could cross a controller switch and read the newly selected controller token — FIXED + ANDROID/HOST CI VALIDATED.** Command/login network work now captures the target controller and API token together, watches `SettingsStore` selection changes while cancellable HTTP work is in flight, and cancels that request if selection changes. A request to controller A can no longer dynamically read controller B's API token after a switch. Build #1213 Android + host jobs passed on the exact code head.

### MEDIUM
- **A-004 — MainActivity remains a large orchestration object — OPEN / DEFERRED.** Do not mechanically split it before runtime contracts are frozen; adding layers now risks recreating the parallel runtimes just removed.
- **A-005 — Global cleartext HTTP enabled — OPEN / CURRENTLY REQUIRED BY CONTRACT.** Setup/manual/fallback discovery intentionally talks HTTP to arbitrary LAN IPv4 targets; Android domain allowlists cannot safely express arbitrary manual/RFC1918 IPs. Revisit only with protocol/endpoint redesign.
- **A-012 — Selected-controller comparison case-sensitive — FIXED.**
- **A-013 — Legacy unnamed devices synthesized `HomeGuard` — FIXED.** UI requires owner rename and shows `Потрібна назва`.
- **A-020 — Device picons mixed authorization/armed/UNKNOWN — FIXED.** Authorization, unknown telemetry and alarm/fault states are distinct.
- **A-044 — `autoReconnect` persistence without behavior — FIXED.** `DeviceSession` includes `autoReconnect` in session target, cancels retry when disabled, and resumes retry only when enabled while OFFLINE. Connection identity remains endpoint+token so toggling policy does not force a needless socket reconnect.
- **A-049 — Stable foreground UDP discovery kept the same 5-second cadence after a controller was already found — FIXED + ANDROID/HOST CI VALIDATED.** First/background-entry scan remains immediate and no-device cadence remains 5 s. Once UDP has at least one controller, periodic cadence becomes 15 s; manual scan is still immediate. Steady foreground UDP work falls from 12 scans/minute to 4 scans/minute (**−66.7%**). Automatic discovery of an additional UDP-only controller while one is already known may wait up to the 15-second steady interval; explicit Add Device scan remains immediate. Build #1213 Android + host jobs passed on the exact code head.

### CLEANUP
- **A-006 — Parallel/dead Android architecture — FIXED + CI VALIDATED.** Removed five `ua.homeguard.app.alarm...` files, `HomeGuardApi`, `DeviceDtos`, `HomeGuardRepository`, `MainUiState`, `MainViewModel`, and old `AlarmAcknowledgementMappingTest`: **11 files / 334 lines**, including **293 production lines**. Build #1143 passed.
- **A-021 — RegisteredDeviceStore process-global `activeStore` bridge — FIXED.** Static registry mutation hooks removed; the app-owned store is passed explicitly.

## Quantitative audit metrics
Only measured repository/CI numbers are reported. Phone-only performance numbers are intentionally not invented.

### Code / structure
- Base: `e522dc7ffc1a716f370ac7050b58c6f1a23e618a`.
- Frozen code/test head before this documentation update: `e35d8dd230ecf254e2857df9788ab9f2274835a1`.
- At that head PR #53 was **136 commits ahead**, **64 changed files**, **2,103 additions / 904 deletions** across production, tests and audit material. These totals are not a pure production-LOC metric.
- Exact source comparison measured on the immediately preceding code/test head (`ca94c5ff...`): Android `src/main` changes were **1,185 additions / 839 deletions = net +346 production lines**. Tests were **692 additions / 41 deletions = net +651 test lines**. Therefore this audit does **not** claim the whole production tree became smaller by LOC.
- Proven-dead island removed: **11 files / 334 lines**, including **293 production lines**.
- `DeviceEndpointResolver` was reduced to read-only resolution; its duplicate write/reconcile branch was removed.
- The main measurable lightweighting is runtime work/I/O reduction rather than a fabricated global LOC reduction.

### APK / resource measurements
- Build #1155 `MyFist-Android` artifact ZIP: **9,611,209 bytes**; extracted APK: **10,018,339 bytes**.
- Build #1207 `MyFist-Android` artifact ZIP: **9,617,366 bytes**; extracted APK: **10,034,723 bytes**.
- #1207 versus #1155: APK **+16,384 bytes (~+0.16%)** while adding the later lifecycle/session/multi-controller safeguards. This is an audit-build comparison, **not** an exact-main/base comparison.
- No matching exact-base Android artifact was found, so no fake base→final APK percentage is reported.
- `bruce_launcher.jpg`: **3,095 bytes, 96×96**; approximate 32-bit decoded bitmap footprint is **36,864 bytes (~36 KiB)**, so Bruce is not a meaningful memory-size target.

### Runtime-contract numbers
- Periodic foreground UDP discovery with no device: **5 s cadence**; first scan is immediate.
- Periodic foreground UDP discovery after at least one UDP controller is found: **15 s cadence**, reducing steady periodic UDP scans **12/min → 4/min (−66.7%)**.
- Normal Activity background: periodic mDNS/UDP/freshness work **0 by lifecycle contract**; session/telemetry intentionally remains available.
- Discovery freshness: **30 s**, foreground expiry tick **1 s**, resume grace **3 s** before expiring cached reports.
- Registry last-seen persistence throttle: **60 s**; timestamp-only persistent-write upper bound from original 5-second refresh is **12/min → 1/min/controller**.
- Manual scan: **single-flight** and immediate; HTTP subnet scan up to **32 probes** in parallel.
- HTTP discovery body cap: **64 KiB characters**.
- Provisioning Wi-Fi scan body cap: **256,000 bytes** on both Content-Length and EOF/body paths.
- Provisioning handoff rediscovery: bounded child scan loop, **5 s cadence**, cancelled when the device is found or the 60-second timeout/coroutine ends.
- Provisioning submit: **one active run maximum**.
- Telemetry reconnect backoff: **2 / 5 / 10 / 20 / 30 s**, capped at 30 s; `autoReconnect=false` disables retry.
- Live-event intermediate buffer: old lossy capacity **16 events** → accepted unique events now queue for the single persistent-history/notification consumer instead of being silently dropped at that threshold.
- Event history cap: **256 records**; after store construction append performs **0 persisted-history reads** versus one full persisted JSON read/parse per append previously.
- NSD and WebSocket stale-callback policy: only current generation accepted; stale-generation callbacks accepted: **0 by contract**.
- Factory Reset: **one destructive request maximum** through the process-wide run gate; completion affects only the controller the request targeted.
- Login/command target binding: selected-controller change during cancellable HTTP work cancels that request; target API token is immutable for the request lifetime rather than dynamically reread from global settings.

## CI status chronology relevant to current code
- Build #1143: dead-code island removal green.
- Build #1146: telemetry setup-failure containment green.
- Build #1153: stale-discovery expiry/manual-IP/lifecycle stage green.
- Build #1155: NSD first hardening stage green.
- Build #1172: A-030..A-036 code/test set green.
- Build #1177: exact head `1704247e...` green before the final lifecycle/device-switch pass.
- Build #1186: host validation caught a test-contract mismatch after generation-aware NSD change; the contract was updated to validate semantics rather than the obsolete literal function signature.
- Build #1207: host validation and Android debug APK jobs passed; produced the measured APK above.
- **Build #1213 on frozen code head `e35d8dd2...`: Host validation = SUCCESS and Android debug APK = SUCCESS.** Android code validation is complete for this head.
- **Build #1215 on current documentation-only head `51305427...`: HomeGuard-S3 Build = SUCCESS.** No production Android code changed after frozen head `e35d8dd2...`.

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
9. **Multi-controller event soak:** produce equal event sequence numbers on two controllers; verify both persist, both notify independently, and each Dashboard shows only its controller history.
10. **Factory Reset switch race:** start confirmed reset on controller A, switch to B before completion, and verify B remains selected while A is marked unauthorized/reset.
11. **Command switch race:** start a local command/login against controller A, immediately switch to B, and verify the old HTTP call is cancelled or returns controller-changed, never uses B's token, and never reports a successful A result as if it belonged to B.

## Freeze decision
- Android PHD/code cleanup is **COMPLETE/FROZEN** at production code head `e35d8dd230ecf254e2857df9788ab9f2274835a1`.
- Frozen code head passed exact-head Host validation + Android debug APK in Build #1213; current documentation-only head `51305427...` passed Build #1215.
- `main` remains untouched; PR #53 remains draft/unmerged.
- Further Android production-code changes require a new concrete defect or failed acceptance/phone benchmark, not speculative refactoring.
- Next phase is the single controlled phone benchmark/acceptance run and filling real cold/warm startup, PSS/RSS/heap, CPU, discovery and reconnect measurements.

## Audit rule
Do not merge to `main` during audit. Keep all work isolated on `audit/android-full-20260818` / draft PR #53 until phone validation passes and a deliberate merge decision is made.