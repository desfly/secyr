# HomeGuard-S3 audit journal — continuation 08

Date: 2026-08-22 (Europe/Kyiv)
Repository: `desfly/secyr`

## Hardware failure after Build #1860

Build #1860 was flashed successfully with matching local SHA256 and `SHA256SUMS.txt`, but the onboard RGB did not light at all on physical RST/EN. Video review confirms no WHITE and no RED from the addressable RGB.

## Root cause found in authoritative Git history

The previous recovery attempt incorrectly treated Build #1813 / commit `22d9f1e804b33d890deca54fdb38595d171ea0ac` as the complete hardware-working reset/RGB baseline.

PR #72 contains the authoritative hardware follow-up from 2026-08-21:

- Build #1813 proved that both cold boot and physical HW-678 RST/EN are reported as `POWERON`;
- the application logged that reset reason 1 was not a physical RST gesture step;
- `RTC_NOINIT` does not survive the board's EN reset path;
- therefore WHITE was never requested on Build #1813; the failure was detector-side, not RGB-driver-side.

The proven repair was Build #1821 / commit `2c81591e5356a1c048691053bdac80796e1b4d59`:

- replace RTC marker detection with persistent NVS boot baseline `hg_rstseq/boot_seen`;
- first boot without the marker establishes the baseline and is not counted;
- later POWERON/EN resets can advance the physical RST sequence;
- each accepted step shows WHITE for 1500 ms;
- third step stages Factory Reset;
- successful erase shows RED for 5 seconds.

PR #72 records hardware PASS for Build #1821 with the exact observed sequence:

`WHITE x3 -> RED x1`

Artifact for that historical PASS was `HomeGuard-S3-firmware`, artifact ID `9456390151`, digest `sha256:6874d9fc68336d35efb814c9b70c169f0608746b91ec53050c1687a458a0d164`.

## Corrective action now

Restore the Build #1821 detector path exactly while leaving the RGB driver untouched:

- `firmware/esp-idf/main/hg_reset_sequence.cpp` -> Build #1821 NVS `boot_seen` implementation;
- `firmware/include/homeguard/reset_sequence.hpp` -> Build #1821 persistent-marker semantics;
- `tests/test_reset_sequence.cpp` -> Build #1821 tests;
- `tools/check_reset_rgb_contract.py` -> reject RTC detector regression and require NVS baseline;
- `tools/check_access_boundary.py` -> same hardware-proven reset security contract.

The RGB driver remains the original hardware-working implementation with 10 MHz RMT, 3/9 and 9/3 timings, GPIO48, WHITE `FF FF FF`, RED `00 FF 00`, reset/latch 80 us. Do not modify it during this repair.

## Error ownership / process rule

The regression was caused by an incorrect rollback target: restoring the Build #1813 RGB source without restoring the later Build #1821 hardware-proven RST detector reintroduced the known RTC assumption that prevents WHITE from being requested.

Future rule: when hardware history contains a later explicit PASS, restore that complete proven behavior chain rather than an earlier partial source snapshot. Hardware PASS records override speculative code-level assumptions.

## Next action

1. Commit the exact Build #1821 reset detector path on current `main`.
2. Let CI build a new firmware artifact.
3. Verify the new run head SHA, jobs, artifact and internal `homeguard_s3.bin` SHA256.
4. Flash only after hash verification.
5. First physical check: one short RST/EN must produce WHITE ~1.5 s.
6. Only after that passes, test three-step sequence and require final RED ~5 s.
