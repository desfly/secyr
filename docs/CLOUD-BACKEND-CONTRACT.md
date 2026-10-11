# Direct MQTT command contract

No external signer service, account server, VPS, Docker deployment or HTTPS
command endpoint is part of this project. Android communicates with the existing
MQTT broker; local Wi-Fi and BLE command paths remain available.

## Current firmware contract

The controller currently accepts a version 1 ECDSA signed envelope on its
`homeguard/<deviceId>/commands` topic and responds on `responses`. Required fields:
`"version"`, `"deviceId"`, `"requestId"`, `"actor"`, `"command"`, `"keyEpoch"`, `"counter"`,
`"issuedAtMs"`, `"expiresAtMs"`, `"challenge"`, `"signature"`.
The signature transcript contains these fields except `"signature"` in that exact
order as `key=value` lines, without a trailing newline. The signature is DER
ECDSA/SHA-256 encoded as hex. Key epoch must match the enrolled trust generation.
Firmware verifies the enabled user's role, expiry, device binding and durable
replay admission before performing an action. The user PIN is never a command field.

## Android

`LocalCommandSigner` signs the same transcript on the phone, using an injected
local private key and durable counter allocator. It rejects another device or
invalid challenge before accessing the key or allocating a counter. No network
request is involved in signing. `MqttSignedCommandController` serializes actions,
obtains/executes the disarm challenge and stops on rejection or unknown outcome.
`RemoteTransportRuntime.configureMqttSigner` now accepts a local signing callback;
it has no URL, account token or HTTP client. Device scope and MQTT health are checked
before calling it. Object-card commands can use this route before local re-login.

The server implementation, deployment assets, server configuration UI, persisted
server settings API, HTTP signer client and associated CI job have been removed.
No migration erases any device, phone, broker, zone or replay configuration.

## Phone enrollment and current limits

`PhoneMqttSignerStore` stores non-exportable P-256 private keys in AndroidKeyStore
per device/actor. Activation requires the controller-confirmed matching public key,
key epoch and replay floor; it never lowers the phone's persisted per-device counter.
Every allocation uses synchronous commit before signing. Missing/mismatched keys
and stale signer profiles fail closed. `DeviceSession` restores activated phone
signers after startup/device selection; activation refreshes only the route, not MQTT.

After an authenticated local HTTP login, the existing MQTT enrollment path reads
`GET /api/v1/cloud/trust`. The response supplies the device id, current public key,
key epoch and actual durable counter floor. It performs no NVS writes and reports
read faults instead of substituting a zero counter. If trust is absent, an Admin
may initialize version 1 using the existing admin-only POST endpoint. Android then
reads it back and activates only a matching phone key. A pre-existing different
key is never overwritten automatically; no counter or configuration is erased.

This initial integration supports the phone whose key is enrolled as the current
controller trust. Another phone/actor key does not silently replace it. Controllers
with a different existing trust key remain unavailable for phone MQTT commands
until a deliberate enrollment/rotation is implemented. Older firmware without the
GET endpoint keeps telemetry working and reports control as not connected. Physical
phone/controller verification remains pending; no hardware has been flashed.

Factory identity, MQTT credentials and Cloud Command Trust identity remain
separate: broker access is not permission to impersonate a controller user.
Firmware enforces signed-command verification and replay admission; these
requirements are guarded by CI. Removing an HTTP signer never bypasses them.
