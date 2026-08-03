# Cloud backend contract

This build includes the ESP32 outbound TLS/MQTT client and Android cloud endpoint routing. It does not include a hosted cloud deployment.

## Device session

- Transport: MQTT over TLS, normally TCP 8883.
- Client ID: stable `device_id`.
- Username: stable `device_id`.
- Password: provisioned random device access token.
- Device subscribes only to `homeguard/<device_id>/commands`.
- Broker ACL must prevent access to every other device namespace.

## Command envelope

The backend must forward only authenticated account commands. Each command must contain:

```json
{
  "requestId": 123456789,
  "issuedAtMs": 1785740000000,
  "expiresAtMs": 1785740120000,
  "command": "arm_away",
  "challenge": null
}
```

The firmware validates authentication at the transport boundary, TTL, request ID uniqueness and dangerous-action challenge before changing outputs.

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
- command expiry and rate limiting;
- audit log for dangerous commands;
- push notification delivery;
- revocation of lost phones and device tokens.
