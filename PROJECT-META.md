# HomeGuard-S3 — PROJECT META

**Status:** CANONICAL / mandatory project requirements  
**Updated:** 2026-09-10

This file defines the permanent goals and mandatory functions of HomeGuard-S3. New firmware, Android, Web UI and hardware work must preserve these requirements unless the user explicitly changes them.

## 1. Main purpose

HomeGuard-S3 is an ESP32-S3 based security, monitoring and home-automation controller with local operation, network access, Android control, event logging and hardware expansion.

## 2. Canonical hardware discipline

- All GPIO assignments are taken only from the canonical HomeGuard-S3 pin map.
- Never declare a GPIO free, occupied or reassign it from memory or assumption.
- Any GPIO change must be verified against the canonical pin map and recorded in Git.
- Bluetooth uses the ESP32-S3 internal radio and must not consume or silently change GPIO assignments.

## 3. Zone monitoring

- 8 zones through 2 × ADS1115.
- A0 of the first ADS1115 = Zone 1, A1 = Zone 2, etc.; mapping must remain explicit in code and documentation.
- Each zone must be monitored live and reported without manual page refresh.
- Mandatory states: NORMAL, OPEN-CIRCUIT, SHORT-CIRCUIT.
- Web UI: all 8 zones visible; admin can rename zones; small status circle: green = normal, red = open circuit, yellow = short circuit.
- Android must receive the same live zone state as Web/API telemetry.

## 4. Relay/output logic

- Output control is centralized so Web, Android, BLE and other transports call the same command logic.
- If Zone 1 or Zone 2 enters alarm, the lighting relay starts its 1-minute cycle.
- If both zones return to normal before the minute ends, the already-started cycle finishes; a new cycle is not started unless a new alarm condition occurs.
- Lock relay is manual-command only and activates for 5 seconds.
- Lock control must be available from Web UI and Android.
- Planned controlled outputs also include lighting and two servo valves, with MCP23017 used for I/O expansion where assigned by the canonical hardware map.

## 5. Network and local communication

HomeGuard-S3 must support multiple communication transports without duplicating business/control logic.

Primary transports:
- Ethernet via W5500.
- Wi-Fi when enabled by the active firmware configuration.
- Local Web/API/WebSocket telemetry path.
- MQTT/cloud path where configured.

### 5.1. Mandatory full Bluetooth channel: Android ↔ ESP32-S3

Bluetooth LE is a permanent, full-featured communication channel, not only a setup helper.

Requirements:
- ESP32-S3 acts as a BLE GATT server/peripheral for the Android app.
- Android acts as BLE central/client.
- BLE must provide local operation when Wi-Fi/Ethernet is unavailable or intentionally unused.
- BLE carries live telemetry and commands through the same internal models/dispatchers used by network transports.
- Mandatory BLE telemetry includes: 8 zone states, events, controller/system status, connection/transport diagnostics, relay/output state and other supported live measurements.
- Mandatory BLE commands include the same authorized local control set as Android over IP: status request, output commands, lock pulse, lighting control, servo-valve commands, supported configuration commands and reboot/configuration actions where security policy allows.
- Android diagnostics must identify BLE as the active transport when BLE is used; it must not remain `NONE` while data is flowing over BLE.
- BLE transport must include connection state, reconnect handling, packet framing/reassembly, write queueing and notification subscription.
- Android 12+ Bluetooth permission handling must use `BLUETOOTH_SCAN` / `BLUETOOTH_CONNECT` correctly and must not request unnecessary location permission.
- Security must include authenticated device association/session handling; sensitive commands must not be accepted from arbitrary nearby devices.
- BLE implementation must be memory-conscious for ESP32-S3; NimBLE is preferred when compatible with the active firmware framework.

### 5.2. Mandatory Bluetooth key-fob channel: BLE remote ↔ ESP32-S3

HomeGuard-S3 must also support BLE key fobs/remotes as a separate lightweight control channel.

Requirements:
- Key-fob support coexists with the Android BLE channel; adding a remote must not remove Android BLE operation.
- A paired/bound remote has its own identity and permission set.
- Every bound remote is assigned to a HomeGuard user and has an admin-visible name.
- Admin UI must show the current number of bound remotes, configured capacity, each remote name, its assigned user, and its permissions/status.
- Admin must be able to add a remote, edit its name/assigned user/permissions, reassign it to another user, and delete it.
- Remote-management changes must require an authorized Admin session and persist in controller storage.
- A remote whose assigned user no longer exists or is disabled must not retain control authority; orphaned remote bindings must be rejected or removed.
- A remote must not receive permissions exceeding the permissions of its assigned user role.
- Supported remote actions may include: arm, disarm, lock pulse, lighting command, SOS/panic and other explicitly assigned commands.
- Remote button actions must be mapped to the same centralized command dispatcher used by Web/Android/BLE, not to duplicate relay logic.
- Remote commands must include replay/duplicate-event protection where the remote protocol permits it.
- Unknown/unpaired BLE devices must not be able to control HomeGuard-S3.
- Because commercial BLE key fobs use different protocols, firmware must use a remote-profile abstraction rather than assume every key fob is compatible.
- Planned remote profiles may include HomeGuard-native/custom GATT, HID-over-GATT and known advertisement/GATT remote profiles after the exact device protocol is verified.
- A specific commercial key fob is not marked SUPPORTED until its real BLE behavior/protocol is captured and verified on hardware.

## 6. Android application invariants

- Start screen is the device list, not an immediate add-device screen.
- Local discovery can automatically find HomeGuard devices.
- Duplicate discovered devices are automatically merged.
- Device name is mandatory before saving.
- Device card does not show technical ID/IP; full technical information belongs in properties/diagnostics.
- Bruce image remains the project/device visual identity; icons stay compact.
- Build/version information must be visible or otherwise clearly traceable for testing.
- All live telemetry transports feed the same `SystemSnapshot`/UI state rather than transport-specific duplicate screens.
- Transport routing must support IP/Ethernet/Wi-Fi and BLE fallback or explicit selection without changing the visible device identity.

## 7. Web UI invariants

- Web UI provides live controller/zone status.
- 8 zones are visible and individually nameable.
- Lock control is present and follows the 5-second lock-pulse rule.
- Admin UI includes BLE remote management with count, user assignment, add/edit/reassign/delete controls and per-remote permissions.
- UI status must reflect real device state, not only successful HTTP button submission.

## 8. Peripheral scope

Project hardware includes or plans integration for:
- ESP32-S3 main controller.
- W5500 Ethernet.
- microSD logging/storage.
- 2 × ADS1115 for 8 zones.
- INA226 power/current monitoring.
- MCP23017 I/O expansion.
- PZEM-004T / mains monitoring where the verified electrical interface is used.
- Sensors including alarm loops, pressure/flood-related inputs and future approved modules.

## 9. Architecture rule

Transport is not business logic.

All transports — Web/API, WebSocket, MQTT, Android BLE and BLE remotes — must enter a common command/authorization layer and read from common telemetry/state models. A new transport must not create a second independent implementation of relay, lock, zone or valve behavior.

## 10. Verification status vocabulary

Use the project discipline states exactly:
- FOUND
- CAUSE FOUND
- CODE READY
- CI PASS
- FLASHED
- HW PASS
- FIXED only after hardware confirmation.

A green build or committed code alone is not proof that a hardware function is FIXED.

## 11. Permanent BLE acceptance target

Bluetooth work is complete only when all of these are demonstrated on real hardware:

1. Android discovers and securely connects to the HomeGuard-S3 over BLE.
2. Android receives continuous live telemetry for all 8 zones over BLE.
3. Android commands relays/lock/valves over BLE through the common command dispatcher.
4. Loss of Wi-Fi/Ethernet does not prevent authorized local Android BLE control.
5. Diagnostics reports BLE as the active transport.
6. A bound BLE key fob triggers only its permitted commands.
7. An unknown/unbound BLE device cannot control the system.
8. Android BLE and BLE key-fob support coexist without breaking Ethernet/Wi-Fi operation.
9. Admin can see the remote count and user assignments and can add, edit/reassign and delete remotes with persistence across reboot.

---

**Rule:** This document is the canonical project meta. Do not silently remove, weaken or replace these requirements. Any intentional change must be explicit and committed to Git.
