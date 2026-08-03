# HomeGuard-S3 APIs — Build-0013

## Provisioning

The first-run endpoints remain:

- `GET /v1/provisioning/info`
- `POST /v1/provisioning/authorize`
- `POST /v1/provisioning/apply`

Provisioning runs on `https://192.168.4.1:8443` and is authenticated by the QR pairing code plus the exact certificate SHA-256 fingerprint.

## Discovery

- mDNS service: `_homeguard._tcp.`
- UDP port: `45678`
- UDP request: `HG_DISCOVER_V1`
- response protocol: `homeguard-discovery-v1`
- normal hostname: `homeguard-s3-xxxxxx.local`
- normal API port: `443`

The advertised hostname is included in the device certificate SAN. Android should prefer it over a numeric DHCP address.

## Authentication

Every operational HTTPS/WSS request must include:

```http
Authorization: Bearer <local_api_token>
```

The device stores the token in encrypted NVS. Runtime services retain only a SHA-256 digest. Missing or invalid credentials return HTTP 401.

## `GET /api/status`

Returns a telemetry snapshot:

```json
{
  "sequence": 42,
  "uptimeMs": 123456,
  "rtcEpoch": 0,
  "mode": "disarmed",
  "transport": "wifi_sta",
  "health": "degraded",
  "failedComponents": 0,
  "crc": 123456789,
  "zones": [],
  "pressures": []
}
```

## `GET /api/health`

Returns overall state, active transport and all monitored components.

## `POST /api/challenge`

Only dangerous commands may request a challenge:

```json
{ "command": "open_valves" }
```

The response includes a 32-bit one-time challenge and expiry in device uptime milliseconds.

## `POST /api/command`

```json
{
  "requestId": "72623859790382857",
  "issuedAtMs": "1785745000000",
  "command": "arm_home",
  "challenge": 123456789
}
```

`requestId` is a decimal string to preserve all 64 bits. `issuedAtMs` is retained for client diagnostics but the firmware uses its own monotonic receive time for replay-window accounting. Dangerous commands require the one-time challenge.

Response:

```json
{ "accepted": true, "duplicate": false, "code": "accepted" }
```

## `WSS /ws/telemetry`

The WSS handshake uses the same bearer token and TLS certificate as REST. The server sends the same JSON shape as `/api/status` at the configured interval. Client-to-device application commands are not accepted over this socket; commands use REST.
