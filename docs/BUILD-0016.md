# HomeGuard-S3 Build-0016

Build-0016 continues from Build-0015.

## Firmware

- alarm acknowledgement is projected into a dedicated telemetry frame;
- alarm sequence and acknowledgement timestamps remain JSON strings;
- acknowledgement actions produce audit events;
- replayed idempotent requests are explicitly marked;
- new portable delivery/audit test;
- new acknowledgement sources are added to the ESP-IDF core component.

## Android

- bounded offline command queue;
- FIFO delivery based on enqueue time;
- exponential retry delay;
- optional server `Retry-After` delay;
- maximum attempt limit and terminal queue state;
- cancellation-safe queue flusher;
- automatic flush when connectivity changes to online;
- duplicate or accepted device replies remove commands from the queue;
- failed transport leaves the command queued.

## Safety

Dangerous commands remain subject to challenge validation on the device.
Queueing a dangerous command does not extend or reuse an expired challenge.
Such a command may be rejected and must be recreated by the user.
