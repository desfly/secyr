# ESP-IDF integration: alarm acknowledgement

Endpoint:

```http
POST /api/v1/alarm/acknowledge
Authorization: Bearer <token>
Content-Type: application/json
```

Request:

```json
{
  "alarm_sequence": "42",
  "actor": "android:device-id",
  "request_id": "uuid-or-monotonic-string"
}
```

Server requirements:

1. Use server receive time, not client time.
2. Require Bearer token.
3. Run normal idempotency checks.
4. Pass the command to `AlarmAckCommandHandler`.
5. Append result to the event/audit log.
6. Publish updated alarm state over WSS telemetry.
7. Do not clear the physical alarm cause.
8. Do not unlock water valves.
