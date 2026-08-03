# Implementation status — Build-0013

## Implemented

- Portable controller, zones, pressure states, event log, maintenance guard, idempotency and challenge logic.
- Secure Setup AP provisioning, encrypted NVS and per-device certificate provisioning.
- Wi-Fi STA startup, reconnect supervisor, mDNS/UDP discovery and outbound MQTT/TLS foundation.
- Authenticated operational HTTPS endpoints `/api/status`, `/api/health`, `/api/challenge`, `/api/command`.
- Authenticated WSS telemetry endpoint `/ws/telemetry` on the same TLS server.
- Exact certificate SHA-256 pinning in Android for provisioning, REST and WSS.
- Android route resolution, local/cloud WSS session, offline command queue and provisioning handoff.
- Host C++ tests, executable pure-Kotlin protocol/route tests, resolver compile gate, ESP-IDF syntax validation and CI build workflows.
- Pinned ESP-IDF managed dependencies and collection of dependency lock/effective build metadata.

## Disabled until hardware verification

- W5500 SPI/CS/INT/RST GPIO map and physical Ethernet driver startup.
- ADS1115 and DS3231 I2C pins, addressing and calibration.
- Alarm-loop GPIOs and end-of-line electrical behavior.
- Siren, valves and auxiliary output GPIOs and active polarity.
- Dedicated service/reset button while its GPIO remains `-1`.

All these adapters fail closed.

## Not completed yet

- Real ESP-IDF link/build result from the pinned CI container.
- Real Gradle lint/test/APK result from the Android CI job.
- Physical flash and boot test on HW-678.
- Real Android-to-device test of Setup AP, Wi-Fi handoff, mDNS, HTTPS and WSS.
- Signed production APK, Secure Boot, application flash encryption and production key ceremony.
- Physical W5500/sensor/input/output drivers after the GPIO map is verified.
