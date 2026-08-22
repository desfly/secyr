# HomeGuard-S3 audit journal — continuation 05

Date: 2026-08-22 (Europe/Kyiv)
Repository: `desfly/secyr`
Previous journal: `AUDIT-JOURNAL-2026-08-22-CONTINUATION-04.md`

## Build #1855 checkpoint

Authoritative GitHub Actions run supplied by the user:

- Workflow: `HomeGuard-S3 Build`
- Run: `32564986337`
- Run number: `1855`
- Branch: `main`
- Head SHA: `6c640c4c961ae31207f40ea6e7ba3c5a0b724755`
- Event: `push`
- Status: `completed`
- Conclusion: `success`
- Firmware artifact: `HomeGuard-S3-firmware`, artifact `9473876245`
- Artifact SHA256: `d9db99f5c27a50788ea93ed0092e8dadce6a28bd8620b09d70c19151c7c5122d`
- `homeguard_s3.bin` SHA256 verified by the user before flashing: `a6eeb1040e7525969388b7dcadb3cb98c803fb1a482c0b554f5ca8c681929d06`

Build #1855 contained implementation commit:

`0d743156fb367168a745d5bd19dc9cf343f2f7f2` — **fix: align onboard WS2812 timing with Espressif reference**

That commit changed the RGB RMT bit timing from the previously hardware-working `3/9, 9/3` ticks to `4/8, 8/4` ticks and updated the CI contract to require the new timing.

## Hardware retest of Build #1855 — FAIL

The user performed a clean hardware deployment:

1. verified the exact Build #1855 firmware SHA256;
2. executed `esptool erase-flash` successfully on COM6;
3. flashed bootloader, partition table, OTA data and `homeguard_s3.bin` successfully;
4. all esptool writes reported `Hash of data verified`.

Real-hardware result after the clean flash:

- onboard RGB did **not light at all** during reboot;
- onboard RGB did **not light at all** on physical RST/EN presses;
- therefore the Build #1855 `4/8, 8/4` timing change is incompatible with this physical board.

This is a direct hardware regression. CI success for Build #1855 did not validate the electrical RGB waveform and must not be interpreted as hardware success.

## Correction of the previous root-cause claim

The previous journal wording that identified `3/9, 9/3` timing as the cause of the final RED problem was speculative and is superseded by the physical Build #1855 result.

Known hardware evidence is now:

- Build-1813 / commit `22d9f1e804b33d890deca54fdb38595d171ea0ac` used `3/9, 9/3` and produced visible WHITE RST acknowledgements on the real board;
- Build #1855 used `4/8, 8/4` and produced no RGB output at all.

Therefore `3/9, 9/3` is the hardware-proven baseline for this board and must be restored.

The earlier final five-second indication appearing WHITE came from phone-video observation and did not justify changing the physical bit timing. Full-brightness color saturation remains a plausible visibility issue, so the next RED test reduces only RED intensity while preserving the proven waveform.

## Corrective implementation

Corrective source change prepared after the Build #1855 hardware failure:

- restore RMT timing to hardware-proven `3/9, 9/3` ticks at 10 MHz;
- keep the 80 us reset/latch interval;
- keep WHITE acknowledgement at `FF FF FF`;
- keep standard GRB RED channel selection but reduce RED intensity to `00 40 00` (25%) so the five-second success indication is visibly chromatic and less likely to saturate camera/highlights;
- do not change the already hardware-validated three-step RST/EN detection, sequence persistence, erase staging or five-second success duration;
- update `tools/check_reset_rgb_contract.py` so CI rejects the hardware-failed `4/8, 8/4` timing and locks the restored hardware baseline.

## Validation status

Still hardware-PASS and not to be reopened without regression evidence:

- three physical RST/EN presses are recognized;
- destructive factory reset occurs;
- stored infrastructure Wi-Fi is erased;
- AP provisioning returns at `192.168.4.1`;
- first-Admin setup returns.

Open item:

- RGB output must first return with the restored timing;
- final success indication must be visibly RED for approximately five seconds.

## Immediate next action

1. Build the corrective firmware containing restored `3/9, 9/3` timing and reduced-brightness RED.
2. Verify CI, obtain the exact new artifact/run/SHA.
3. Flash that artifact on COM6.
4. Confirm WHITE acknowledgement is restored on RST/EN.
5. Complete the three-step sequence and verify final RED for approximately five seconds.
6. Record the physical result before making another RGB change.
