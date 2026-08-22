# HomeGuard-S3 audit journal — continuation 06

Date: 2026-08-22 (Europe/Kyiv)
Repository: `desfly/secyr`
Previous journal: `AUDIT-JOURNAL-2026-08-22-CONTINUATION-05.md`

## Critical correction: the RGB driver was already hardware-proven

The user explicitly corrected the previous diagnosis: the onboard RGB had already worked physically in both WHITE and RED states. Therefore the RGB driver itself was a proven component and should not have been altered while investigating the reset indication.

The assistant's changes to WS2812 timing and later RED brightness were unjustified regressions. The correct recovery action is an exact source rollback, not further tuning.

## Hardware evidence from the two regression builds

### Build #1855

- `homeguard_s3.bin` SHA256: `a6eeb1040e7525969388b7dcadb3cb98c803fb1a482c0b554f5ca8c681929d06`
- clean erase + flash completed successfully on COM6;
- RGB produced no visible output.

### Build #1857

- source head before rollback: `dca7cffe999283e276fedb64e21b952dd2d6f801`
- `homeguard_s3.bin` SHA256: `963c802b968cef4f94e9882e013dc316339cb7fb37617fb367b31f3c2a262a8c`
- SHA matched the artifact's `SHA256SUMS.txt`;
- clean erase + flash completed successfully on COM6;
- after one physical RST/EN press the RGB still produced no visible WHITE acknowledgement.

Do not reinterpret these failures as proof that the historical RGB driver was bad. The historical driver had already worked on this physical board.

## Exact rollback performed

Historical working reference:

- Build-1813 app commit: `22d9f1e804b33d890deca54fdb38595d171ea0ac`
- file: `firmware/esp-idf/main/hg_rgb_diagnostic.cpp`
- historical blob SHA: `0c246d3fb045c408185f1719707dd2947177e8c3`

The current `main` copy of `hg_rgb_diagnostic.cpp` was replaced with the exact historical contents from that commit.

Rollback commit:

- `4c83cd8fe6a3190d8110ef9661f48a09f9d12600` — `fix: restore Build-1813 RGB driver exactly`
- resulting current blob SHA: `0c246d3fb045c408185f1719707dd2947177e8c3`

The identical blob SHA proves the driver is now byte-for-byte the historical Build-1813 version. This restores:

- 10 MHz RMT resolution;
- `3/9, 9/3` bit timing;
- WHITE = `FF FF FF`;
- RED = `00 FF 00` in GRB order;
- OFF = `00 00 00`;
- original 80 us latch/reset interval;
- original transmit/channel lifecycle.

No brightness reduction, no altered timing, no new RGB behavior remains.

## CI contract rollback

`tools/check_reset_rgb_contract.py` was also restored exactly to its Build-1813-era contents so CI no longer hard-codes assistant-invented RGB electrical timing assumptions.

Rollback commit:

- `7427814c5ac56764894b9be70f7a774feccdc089` — `test: restore pre-regression reset RGB contract`
- resulting blob SHA: `a566c829a9f922e3f59adfdbbc51cbcc4961b6d7`

This old checker still protects the intended reset sequence behavior (3 physical RST steps, WHITE acknowledgement, RED 5 s on successful factory reset) but does not attempt to redefine the already-proven RGB driver's electrical waveform.

## Rule for future work

The Build-1813 RGB driver is now a frozen hardware-proven baseline. Do not modify it again unless a new, isolated hardware test directly proves a defect in that exact historical driver.

If a future build using this exact driver still fails to show RGB, investigate build integration, call path, GPIO ownership/configuration, initialization order, or other code/build differences before touching `hg_rgb_diagnostic.cpp`.

## Immediate next action

1. Let CI build the new head containing the exact driver rollback.
2. Verify the new run/artifact belongs to a head at or after commit `7427814c5ac56764894b9be70f7a774feccdc089`.
3. Verify the downloaded firmware SHA before flashing.
4. Test one physical RST/EN press for WHITE.
5. Only after WHITE is restored, test the existing 3-step sequence and RED success indication.
