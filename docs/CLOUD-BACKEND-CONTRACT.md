# Cloud backend contract

This build includes the ESP32 outbound TLS/MQTT client and Android cloud endpoint routing. It does not include a hosted cloud deployment.

## Device session

- Transport: MQTT over TLS, normally TCP 8883.
- Client ID: stable `device_id`.
- Username: stable `device_id`.
- Password: provisioned random device access token.
- Device subscribes only to `homeguard/<device_id>/commands`.
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
  "counter": 42,
  "issuedAtMs": 1785740000000,
  "expiresAtMs": 1785740120000,
  "challenge": "",
  "signature": "<signature over canonical fields>"
}
```

The signature covers, in fixed canonical order, `version`, `deviceId`, `requestId`, `actor`, `command`, `counter`, `issuedAtMs`, `expiresAtMs` and `challenge`. The signature field itself is never part of the signed bytes. `challenge` is always a JSON string: use `""` for commands that do not require a challenge; `security.disarm` uses the currently issued 32-character hexadecimal one-time challenge. `null` is not valid.

Before any command side effect, firmware must fail closed unless all applicable checks succeed:

1. schema/version and field bounds are valid;
2. `deviceId` exactly matches the local stable device identity;
3. the signature verifies against the enrolled Cloud Command Trust public key;
4. the command is fresh: `issuedAtMs` is acceptable and `expiresAtMs` has not passed;
5. `requestId` has not already been accepted;
6. `counter` is greater than the persisted monotonic counter, which is persisted before the side effect;
7. the actor is authorized for the requested command;
8. dangerous actions satisfy their required challenge/presence policy.

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
