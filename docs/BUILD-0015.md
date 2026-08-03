# HomeGuard-S3 Build-0015

Build-0015 continues from Build-0014.

## Implemented

- alarm acknowledgement command handler;
- exact alarm-sequence validation;
- request-id idempotency and replay response;
- server receive timestamp;
- portable REST contract;
- JSON response with string-encoded 64-bit sequence/time;
- Android API contract, controller and UI state;
- portable C++ and Kotlin tests;
- ESP-IDF integration requirements;
- audit/WSS publication requirements.

## Safety invariant

Acknowledging an alarm never:

- changes the physical zone state;
- opens water valves;
- disables smoke, gas or flood monitoring;
- suppresses a new alarm sequence;
- uses client-provided time as authoritative time.
