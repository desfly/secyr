# Build-0013 integration gate

Build-0013 is the first build where discovery points to a real operational local service.

## Expected sequence

1. Android provisions the controller through the Setup AP.
2. ESP32 restarts and joins the home Wi-Fi.
3. mDNS advertises `_homeguard._tcp` with `host=homeguard-s3-xxxxxx.local`.
4. Android verifies the exact device certificate SHA-256.
5. Android opens `wss://homeguard-s3-xxxxxx.local/ws/telemetry` with the local bearer token.
6. REST commands use the same base URL and token.

## Pass criteria for the next physical gate

- GitHub ESP-IDF job produces `.bin`, `.elf`, `.map`, bootloader and partition table.
- GitHub Android job passes unit tests, lint and produces the debug APK.
- Phone receives increasing telemetry sequence numbers for at least 30 minutes.
- Invalid token receives HTTP 401 and cannot complete the WSS handshake.
- A duplicated `requestId` is not executed twice.
- A dangerous command without a challenge is rejected.
- No unresolved output GPIO is driven.
