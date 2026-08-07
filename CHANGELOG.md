# Changelog

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
