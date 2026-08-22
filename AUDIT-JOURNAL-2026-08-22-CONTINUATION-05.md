# HomeGuard-S3 audit journal — continuation 05

Date: 2026-08-22 (Europe/Kyiv)
Repository: `desfly/secyr`
Previous journal: `AUDIT-JOURNAL-2026-08-22-CONTINUATION-04.md`

## Green CI checkpoint after WS2812 RED fix

Authoritative GitHub Actions run supplied by the user:

- Workflow: `HomeGuard-S3 Build`
- Run: `32564986337`
- Run number: `1855`
- Branch: `main`
- Head SHA: `6c640c4c961ae31207f40ea6e7ba3c5a0b724755`
- Event: `push`
- Status: `completed`
- Conclusion: `success`
- Started: `2026-08-22T09:27:23Z`
- Completed: `2026-08-22T09:31:26Z`

This head contains implementation commit:

`0d743156fb367168a745d5bd19dc9cf343f2f7f2` — **fix: align onboard WS2812 timing with Espressif reference**

## Job results

All three jobs completed successfully:

- `Host validation` — SUCCESS
  - importantly, `Physical RST/RGB reset contract` — SUCCESS
- `ESP-IDF 5.4.4 firmware` — SUCCESS
- `Android debug APK` — SUCCESS

## Firmware artifact for hardware retest

Use this artifact, not Build-1813:

- Artifact name: `HomeGuard-S3-firmware`
- Artifact ID: `9473876245`
- Size: `9,934,417` bytes
- SHA256 digest: `d9db99f5c27a50788ea93ed0092e8dadce6a28bd8620b09d70c19151c7c5122d`
- Head SHA: `6c640c4c961ae31207f40ea6e7ba3c5a0b724755`
- Expiration: 2026-11-20

Other green artifacts from the same run:

- `MyFist-Android` — artifact `9473847832`
- `HomeGuard-S3-ESP-IDF-diagnostics` — artifact `9473875911`
- `HomeGuard-S3-host-diagnostics` — artifact `9473846766`

## Next hardware action

1. Download `HomeGuard-S3-firmware` from Build #1855 / run `32564986337`.
2. Flash the ESP32-S3 using the established COM6 command/layout.
3. Retest the already-proven physical 3x RST/EN sequence.
4. The only acceptance point for this retest is the final success RGB indication: it must be RED for approximately five seconds before OFF/reboot.
5. If RED passes, close the RGB defect and continue PC Web UI validation, then mobile.

Do not reopen the already hardware-validated reset detection/factory erase unless a new regression is observed.
