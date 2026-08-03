# Offline command delivery

The Android client may queue commands while the controller is unavailable.

Rules:

1. `requestId` is preserved for idempotency.
2. Accepted and duplicate replies are considered delivered.
3. Transport failures use exponential backoff.
4. The default maximum delay is 60 seconds.
5. The default attempt limit is 12.
6. Terminal commands are retained for diagnostics until explicitly removed.
7. A reconnect event triggers a bounded flush.
8. Dangerous commands are not automatically given a new challenge.
9. Physical alarms and water shutoff continue locally without Android.
