# Changelog

## Build-0037 — 2026-08-08

- Added bounded AccessControl storage for up to eight operators with Admin/User/Guest roles.
- Added salted iterative SHA-256 PIN digests with constant-time digest comparison; raw PINs are never stored by firmware.
- Added a bounded 64-record authorization audit log with allowed/denied decisions and no PIN material.
- Applied role policy before protected command execution and added explicit credential-required/denied results.
- Extended Android command requests with operator ID and session credential while retaining bearer-token authentication and TLS certificate pinning.
- Added Android operator/PIN controls; PIN stays only in process memory and is cleared when the activity is destroyed.
- Disabled command buttons until a valid operator ID and 4–12 digit PIN are present.
- Preserved dangerous-command confirmation and controller challenge requirements.
- Added host tests for users, PIN verification, roles, authorization decisions and audit behavior.
- Updated firmware and Android versions to 0.0.37 / Build-0037.
- Preserved fail-closed GPIO defaults; no unresolved hardware pin was assigned.

## Build-0036 — 2026-08-07

- Integrated a bounded firmware event log with SystemEventBus events.
- Added `/api/v1/system/events` history and continued live WebSocket event delivery.
- Added Android live event parsing, buffering and Dashboard event rendering.
- Updated firmware and Android versions to 0.0.36 / Build-0036.

## Build-0035 — 2026-08-07

- Connected Android Dashboard controls to the existing authenticated command API.
- Added ARM_HOME, ARM_AWAY, DISARM, SILENCE, RESET_ALARM, OPEN_VALVES and CLOSE_VALVES actions.
- Retained controller challenge handling for dangerous commands and fail-closed behavior while offline.
- Updated firmware and Android versions to 0.0.35 / Build-0035.

## Build-0034 — 2026-08-07

- Connected the real Compose Dashboard to MainActivity.
- Added live telemetry/system snapshot rendering for system state, zones and analog channels.
- Updated firmware and Android versions to 0.0.34 / Build-0034.

## Build-0033 — 2026-08-07

- Added real ESP-IDF system HTTP routes and WebSocket event transport.
- Added `/api/v1/system/status`, zones, outputs, partitions and `/ws/system`.
- Updated firmware and Android versions to 0.0.33 / Build-0033.

## Build-0032 — 2026-08-07

- Added System API v1 JSON serializers for status, zones, outputs, partitions and event frames.
- Added indexed read-only System Core accessors for bounded API snapshots without exposing mutable containers.
- Added host tests covering zone/output/partition snapshots and event JSON.
- Compiled System API directly into the ESP-IDF component so real firmware CI validates it.
- Updated firmware and Android versions to 0.0.32 / Build-0032.
- Preserved fail-closed GPIO defaults; no unresolved hardware pin was assigned.

## Build-0031 — 2026-08-07

- Added a fixed-capacity System Core model for zones, sensors, outputs and partitions.
- Added a bounded event bus with 32 queued events, 8 subscribers, monotonic sequence numbers and overflow accounting.
- Added state-change event generation for zones, outputs and partition arming.
- Compiled the new core directly in the ESP-IDF component so both mock-link and real ESP-IDF CI validate it.
- Updated firmware and Android versions to 0.0.31 / Build-0031.
- Preserved fail-closed GPIO defaults; no unresolved hardware pin was assigned.

## Build-0030 — 2026-08-07

- Synchronized firmware and Android versioning to 0.0.30 / Build-0030.
- Confirmed the main GitHub Actions pipeline builds ESP-IDF 5.4.4 firmware, Android debug APK, host validation and four downloadable artifacts.
- Updated Android CI from `actions/setup-java@v4` to `actions/setup-java@v5` and verified the follow-up main build succeeded.
- Preserved fail-closed GPIO defaults and the existing secure local API, WSS telemetry, discovery and provisioning architecture.

## Build-0013 — 2026-08-03

- Deferred Setup AP shutdown for 1.5 seconds after a successful `/v1/provisioning/apply`, so the HTTPS response can reach Android before Wi-Fi is stopped and the controller restarts.
- Added a portable `ProvisioningShutdownGate` with host tests for arming, due-time and clearing behavior.
- Moved WSS telemetry transmission onto the ESP HTTP server task through `httpd_queue_work`; HTTPS server APIs are no longer called directly from the operational supervisor task.
- Added owned telemetry work buffers so queued WebSocket frames remain valid until the HTTP task finishes sending them.
- Added failed-client cleanup after queued WSS sends.
- Reworked Wi-Fi STA state sharing to atomics and protected IPv4 state with a mutex.
- Added complete Wi-Fi initialization rollback: handler unregister, Wi-Fi deinit, netif destruction and in-memory password clearing.
- Updated Android to 0.0.13 / versionCode 13.
- Retained fail-closed GPIO defaults and pinned ESP-IDF managed dependencies.

## Build-0012 — 2026-08-03

- Fixed Android coroutine and offline-queue compile defects.
- Added full Android source compilation against controlled SDK stubs.
- Added ESP-IDF mock-link coverage for all application/adapter translation units.
- Added verified Gradle 8.9 bootstrap for Windows.

## Build-0009 — 2026-08-03

- Added fail-closed hardware capabilities and explicit unavailable adapters.
- Added ESP task watchdog integration and hardened CI diagnostics.
