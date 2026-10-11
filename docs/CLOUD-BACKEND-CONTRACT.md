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

## Remaining integration

The local signing primitive is tested, but phone key storage/enrollment and durable
counter synchronization are not yet wired into ordinary login. Consequently the
MQTT command route is not automatically enabled and remote buttons are not yet
claimed fixed. An existing trust key must not be overwritten and replay counters
must not be guessed or reset. The next integration must bind the locally enrolled
phone key to authorized users and preserve existing NVS/configuration. This requires
code work on the phone/controller, not a separate server.

Factory identity, MQTT credentials and Cloud Command Trust identity remain
separate: broker access is not permission to impersonate a controller user.
Firmware enforces signed-command verification and replay admission; these
requirements are guarded by CI. Removing an HTTP signer never bypasses them.
