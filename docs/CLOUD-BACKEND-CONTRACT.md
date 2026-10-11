# Cloud backend contract

This build includes the ESP32 outbound TLS/MQTT client and Android cloud endpoint routing. It does not include a hosted cloud deployment.

## Device session

- Transport: MQTT over TLS, normally TCP 8883.
- Client ID: stable `device_id`.
- Username: stable `device_id`.
- Password: provisioned random device access token.
- Device subscribes only to `homeguard/v1/devices/<device_id>/commands`.
- Broker ACL must prevent access to every other device namespace.
- Broker authentication and ACL are transport controls; they do not replace command-envelope authentication by the device.

## Canonical command envelope

There is one command schema for firmware, backend and CI. The backend must accept commands only from an authenticated account bound to the target device and must sign the canonical command envelope with the enrolled Cloud Command Trust key.

```json
{
  "version": 1,
  "deviceId": "HG-S3-7A31BC",
  "requestId": "01K5EXAMPLE",
  "actor": "account-or-user-id",
  "command": "security.arm_away",
  "keyEpoch": 1,
  "counter": 42,
  "issuedAtMs": 1785740000000,
  "expiresAtMs": 1785740120000,
  "challenge": "",
  "signature": "<signature over canonical fields>"
}
```

The signature covers, in fixed canonical order, `version`, `deviceId`, `requestId`, `actor`, `command`, `keyEpoch`, `counter`, `issuedAtMs`, `expiresAtMs` and `challenge`. The signature field itself is never part of the signed bytes. `challenge` is always a JSON string: use `""` for commands that do not require a challenge; `security.disarm` uses the currently issued 32-character hexadecimal one-time challenge. `null` is not valid.

Before any command side effect, firmware must fail closed unless all applicable checks succeed:

1. schema/version and field bounds are valid;
2. `deviceId` exactly matches the local stable device identity;
3. `keyEpoch` exactly matches the active monotonic Cloud Command Trust version, so rotation invalidates envelopes signed for an older key generation;
4. the signature verifies against the enrolled Cloud Command Trust public key;
5. the command is fresh: `issuedAtMs` is acceptable and `expiresAtMs` has not passed;
6. `requestId` has not already been accepted;
7. `counter` is greater than the persisted monotonic counter, which is persisted before the side effect;
8. the actor is authorized for the requested command;
9. dangerous actions satisfy their required challenge/presence policy.

The firmware enforces signed-command verification, trusted-time freshness, local actor authorization, persistent monotonic counter/request-ID replay barriers, and one-time challenge enforcement for remote disarm; these contracts are guarded by CI.

## Trust separation

Three identities are intentionally separate:

- **Factory identity** proves which physical controller this is. Its private key is manufacturing material and is not a Cloud command signing key.
- **MQTT credentials** authorize a device session at the broker and are scoped by broker ACL to that device namespace.
- **Cloud Command Trust identity** authorizes command envelopes. Firmware stores only the enrolled public key/certificate; the Cloud signing private key never resides on the controller.

The Cloud Command Trust key may be installed or rotated only through an authenticated enrollment/administrative flow with explicit authorization. Ordinary MQTT payloads and ordinary broker configuration changes must not be able to replace it.

## Android HTTPS route

After account sign-in, Android addresses the selected device through:

```text
https://<cloud-api>/v1/devices/<device_id>/api/...
wss://<cloud-api>/v1/devices/<device_id>/ws
```

The backend maps that route to the authenticated device session and never reveals the controller's private LAN address.

## Required backend controls

- per-account device binding;
- short-lived user access tokens;
- broker ACL isolation;
- Cloud Command Trust key lifecycle and rotation audit;
- command expiry and rate limiting;
- audit log for dangerous commands;
- push notification delivery;
- revocation of lost phones and device tokens.

## Android MQTT command readiness

Android receives live compact state from `homeguard/v1/devices/<device_id>/state`.
This is telemetry readiness, not command readiness. MQTT remains excluded from
command selection until a server signing route and account authentication are configured for the selected device.
`MqttRuntimeClient.publishSignedCommand` accepts an immutable `SignedCommandEnvelope`
with the firmware's exact v1 fields and sends it unchanged at QoS 1 without retention.
It rejects mismatched device IDs and expired packets before publication. It does not
verify the server signature locally; firmware remains the authority for signature,
actor authorization and durable replay admission. There is no MQTT PIN command API. Pending command completion requires a live,
non-retained response with the exact `requestId` and a boolean `ok`. Timeout,
connection loss and session stop after publication report `MqttCommandOutcomeUnknown`;
callers must not interpret this as rejection or automatically retry the action.
Explicit caller coroutine cancellation still propagates as cancellation.
The repository supplies a configurable signer implementation in `backend/` and
an Android HTTPS `SignedCommandApi` client. The client sends only command/challenge
with a separate account bearer token, validates the returned device/command and
lifetime, disallows redirects/retries, bounds response size and cancels HTTP work
when its coroutine is cancelled. It returns a packet for `publishSignedCommand`;
it does not publish commands itself. The daily-action controller is wired to the optional MQTT route, but signer
URL/account-token provisioning and a hosted deployment are not included.

## MQTT disarm coordinator

`MqttSignedCommandController` serializes requests for one configured device.
Disarm first obtains and executes a signed `security.disarm_challenge`, checks
the successful correlated response and its 32-character token/TTL, then signs
and publishes `security.disarm` with that token. Rejection, missing/malformed
challenge, or unknown outcome stops the sequence without retry. Firmware now
returns the token and `ok` in the same `requestId`-correlated response rather
than an uncorrelated token plus a separate success acknowledgement.
The coordinator still needs signer credentials and a deployed endpoint. It does not enable MQTT commands merely because telemetry is online.

## Daily-action routing

`MonitoringRuntime.commands` installs `MqttCommandRoute` into the actual
`CommandController` used by daily buttons. After the existing ready BLE path,
Arm Home/Away, Disarm and Lock can use a configured, fresh MQTT route when the
selected endpoint is CLOUD or OFFLINE. Local HTTP behavior is retained. A
non-null MQTT result, including rejection or unknown outcome, stops routing;
there is no automatic second-transport attempt after a potentially sent action.

The dashboard's remote control panel accepts an HTTPS signer URL and a separately
issued account token for the selected device. `SettingsStore` stores the URL per
device and the token with the existing AndroidKeyStore-backed `SecureTokenStore`;
neither is included in settings backup. The token input starts empty and is
cleared after saving. Removing access deletes both values from the phone.
`DeviceSession` restores the route after MQTT setup, updates it without restarting
MQTT when credentials change, and checks the selected device before signing.
Stopping monitoring clears the in-memory route; saved credentials are restored
on the next start. Invalid or incomplete saved configuration disables the route.
The low-level `configureMqttSigner` API remains available for account integration.
Deployment, account/grant provisioning and real end-to-end validation remain open.

Quick actions on object cards attempt the account-authorized MQTT route before
local PIN re-login when no BLE session is ready and the endpoint is CLOUD/OFFLINE.
A rejection or uncertain result ends the action. Only an unavailable/unconfigured
route proceeds to the existing saved-login flow. Signed-actor session permission
on the firmware remains mandatory; this does not enroll or authenticate an actor.
