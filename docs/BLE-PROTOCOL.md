# HomeGuard-S3 BLE protocol

Status: CODE CONTRACT / stage 1

HomeGuard BLE is an additional transport for the same controller state and command model used by IP transports. It does not own relay, zone, lock or valve business logic.

## GATT service

The first implementation uses the Nordic-UART-compatible UUID triplet as a transport envelope while the payload remains HomeGuard-specific:

- Service: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- RX (Android -> controller, write): `6e400002-b5a3-f393-e0a9-e50e24dcca9e`
- TX (controller -> Android, notify): `6e400003-b5a3-f393-e0a9-e50e24dcca9e`

Using these UUIDs does not make the protocol Nordic UART; only the GATT transport layout is compatible.

## Packet framing

Every RX/TX GATT value is one fragment:

| Byte | Meaning |
|---|---|
| 0 | protocol version (`1`) |
| 1 | message type |
| 2..3 | message id, little-endian |
| 4 | fragment index, zero-based |
| 5 | fragment count |
| 6.. | UTF-8 JSON payload fragment |

Message types:

- `1` telemetry snapshot
- `2` event
- `3` command
- `4` command reply
- `5` hello/session
- `6` remote/key-fob event
- `7` error

Receivers must reject wrong version, zero fragment count, index >= count, inconsistent message ids/types, duplicate conflicting fragments and assembled payloads larger than 4096 bytes.

The Android client requests an ATT MTU of 247. The sender still derives its payload size from the negotiated MTU and never assumes the request succeeded.

## Payload rules

JSON field names for telemetry stay identical to the existing WebSocket telemetry JSON. Therefore the Android `JsonParsers.snapshot()` parser is shared by WebSocket and BLE.

Commands use the existing HomeGuard command vocabulary and authorization path. A BLE transport may authenticate a session and carry a command, but must not directly manipulate GPIO or relay state.

## Security

Controller BLE links must use bonding/encryption. Sensitive commands additionally require an authenticated HomeGuard session/credential accepted by the same authorization layer as other local transports. Unknown/unbound devices do not receive control authority.

## BLE remotes/key fobs

Commercial remotes are represented by profile adapters. The core remote event is normalized before command dispatch and contains remote identity, button/action, monotonic/replay information when the source protocol supplies it, and the mapped HomeGuard permission. No commercial remote is declared supported until its real protocol has been captured and hardware-tested.
