# HomeGuard-S3 BLE reliability audit — 2026-09-13

## Scope

Android `BleHomeGuardClient`, `BleRuntimeSession` and BLE discovery at PR #95
head `4e915e42f0d38944b93babb5a9e0952edc600ca8`. The fix is deliberately
Android-only so the existing ESP 2404 hardware-test firmware can remain flashed.

## Observed hardware trace

```text
SECURITY_PROBE -> SECURITY_WAIT -> ... -> SECURITY_ERROR
-> RETRYING -> SCAN_PRECHECK -> SCANNING -> SCAN_TIMEOUT
```

## CAUSE FOUND

`BleRuntimeSession.connect()` retried by calling `scanner.find()` before closing
the failed `BluetoothGatt`. The ESP therefore still had an active peripheral
connection and correctly did not advertise. The retry scan could not rediscover
the controller and ended in `SCAN_TIMEOUT`.

The client also did not call `BluetoothGatt.close()` after a remote disconnect,
leaking Android GATT client resources and increasing the risk of later generic
status 133 failures.

The encrypted ATT probe allowed only six attempts at 500 ms spacing (roughly
three seconds). That is shorter than normal bonding time on some Android vendor
stacks and caused a premature `SECURITY_ERROR` while the enclosing connection
window was still active.

## CODE READY

- Enforce `failed attempt -> close GATT -> backoff -> next scan` ordering.
- Close remotely disconnected `BluetoothGatt` instances.
- Preserve one diagnostic chain across cleanup/retry.
- Close any stale runtime GATT before the first scan, not only between retries.
- Extend the encrypted ATT readiness window to twelve attempts at 750 ms.
- Use the Android 13+ immutable-value descriptor/characteristic write APIs.
- Make BLE protocol message IDs atomic.
- Return an immediate failure when the first queued GATT write cannot start.
- Dispatch GATT callbacks on one looper to remove vendor callback races.
- Serialize connection, authentication and request/reply operations.
- Invalidate the link after an authentication or command timeout so a late
  reply cannot satisfy a later operation.
- Require both the HomeGuard service UUID and the selected controller's exact
  six-character BLE-name suffix; never connect to an arbitrary nearby panel.
- Remove the obsolete bond-state receiver and the arbitrary 400 ms auth delay.
- Add behavior tests for retry ordering, final-failure cleanup and cancellation.
- Add discovery identity-matching tests.

## Verification

- BLE runtime source slice compiles with Kotlin 2.1.0 against Android API 34.
- `BleConnectRetrier` executable behavior harness: PASS.
- `BleAdvertisementMatcher` executable behavior harness: PASS.
- Android access lifecycle audit: PASS.
- Android discovery contract: PASS (22 checks).
- Firmware preflight: PASS.
- ESP-IDF source audit: PASS (existing dynamic-allocation review warnings only).

Full Gradle/ESP-IDF CI and hardware validation remain required.

## Status

`CAUSE FOUND` / `CODE READY` — not `FIXED` until exact artifacts pass CI, are
installed/flashed, and the real device reaches `SECURITY_READY -> AUTHENTICATING
-> READY`, survives reconnect, and continues telemetry/command operation.

## Hardware acceptance for this defect

1. Keep the currently tested ESP 2404 firmware unchanged.
2. Build, record and install the exact Android APK containing this change.
3. Run with Wi-Fi and mobile data off and Bluetooth on.
4. Confirm first connection reaches `READY`.
5. Force a disconnect during security negotiation and confirm the trace includes
   GATT cleanup followed by a successful rediscovery instead of `SCAN_TIMEOUT`.
6. Run at least 100 connect/authenticate/disconnect cycles before marking the
   connection path `HW PASS`.
