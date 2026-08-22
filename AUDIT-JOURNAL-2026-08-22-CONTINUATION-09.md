# HomeGuard-S3 audit journal — continuation 09

Date: 2026-08-22 (Europe/Kyiv)
Repository: `desfly/secyr`

## Recovery completed after Build #1860 RGB-no-light regression

Authoritative project history in PR #72 was re-read before any further hardware attempt.

The key correction is that Build #1813 was NOT the complete hardware-working physical-RST baseline. It still used the invalid RTC_NOINIT detector. HW-678 reports both cold boot and physical EN/RST as POWERON, and RTC_NOINIT does not survive the EN reset. On that implementation WHITE could therefore remain unrequested even though the RGB driver itself was valid.

The first explicit hardware-PASS for the current three-step contract is Build #1821, commit:

`2c81591e5356a1c048691053bdac80796e1b4d59`

PR #72 hardware record for Build #1821:

`WHITE x3 -> RED x1`

## Exact restored source

Current main now contains the exact Build #1821 blobs for the complete physical-RST detector path:

- `firmware/esp-idf/main/hg_reset_sequence.cpp`
  - `dd12150a58fabf0e99f5bf5d8172648d013ffd4a`
- `firmware/include/homeguard/reset_sequence.hpp`
  - `a01ae4553f72a78531922e2cc6cdee13a59af4d1`
- `tests/test_reset_sequence.cpp`
  - `138c069a9a4fd6ebadcb557be1147e3fb259a2c3`
- `tools/check_reset_rgb_contract.py`
  - `0d0d1f5b751627b05202eb8208de9a6203bf87a5`
- `tools/check_access_boundary.py`
  - `8f0563f7dfdcaba0feea9fab847ad6e0b3dac3fc`

The four latter blobs were attached directly by their existing Git blob SHAs from Build #1821, avoiding manual reconstruction.

Implementation commit for the exact five-file detector restoration:

`1731db9a98f7b9da3b473454c721e5ee56b38ae4` — `fix: restore Build 1821 RST detector contract`

(The runtime source was first restored in parent commit `3754601b07bb61d4f2d972cdda6ebaa886afd0b9`, then the remaining four exact Build #1821 blobs were committed atomically.)

## RGB driver is deliberately untouched

Current onboard RGB driver remains the original hardware-known implementation:

- `firmware/esp-idf/main/hg_rgb_diagnostic.cpp`
- blob SHA `0c246d3fb045c408185f1719707dd2947177e8c3`
- GPIO48
- 10 MHz RMT
- T0 = 3/9 ticks
- T1 = 9/3 ticks
- reset/latch = 80 us
- WHITE = `FF FF FF`
- RED (GRB) = `00 FF 00`

No further RGB-driver changes are allowed without new direct hardware evidence.

## Why Build #1860 showed no RGB

Build #1860 had restored the RGB driver but still had the disproved RTC_NOINIT physical-RST detector. Since the physical EN/RST was not recognized as an accepted gesture step on this board, `RgbDiagnostic::set_white()` could remain uncalled. This explains a completely dark RGB without implying a dead GPIO or RMT driver.

## Next hardware checkpoint

1. Wait for CI on the final journal head; use only a green `HomeGuard-S3 Build` whose head includes commit `1731db9a98f7b9da3b473454c721e5ee56b38ae4`.
2. Verify firmware artifact and internal `homeguard_s3.bin` SHA256 before flashing.
3. Clean flash on COM6.
4. After erase-flash, allow the first normal boot to establish NVS `hg_rstseq/boot_seen`; this first boot is intentionally not a gesture and need not show WHITE.
5. Then press physical RST/EN once. Required hardware result: WHITE for about 1.5 seconds.
6. Only after single-step WHITE passes, perform the full three-step sequence and require final RED for about 5 seconds after successful Factory Reset.
