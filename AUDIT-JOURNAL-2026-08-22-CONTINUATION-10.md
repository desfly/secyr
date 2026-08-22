# HomeGuard-S3 audit journal — continuation 10

Date: 2026-08-22 (Europe/Kyiv)

## Corrected hardware-video interpretation for Build #1876

The previous assistant interpretation incorrectly counted multiple video frames of the same illumination as separate RGB events.

Authoritative observed sequence from the hardware video:

1. Power is applied.
2. The small red power/status LED turns on.
3. The onboard addressable RGB turns WHITE once.
4. About 5 seconds later, the user presses and releases the physical RST/EN button once.
5. The onboard addressable RGB turns WHITE a second time.

Therefore this video contains exactly TWO distinct RGB WHITE activations.

There is no third WHITE, no fourth WHITE, and RED is not tested in this video.

## Correct technical interpretation

The first startup WHITE is not a separate mystery caller. It is produced by the restored Build #1821 NVS reset detector itself once `hg_rstseq/boot_seen` already exists.

HW-678 reports both a true cold power-up and a physical RST/EN press as `POWERON`. The Build #1821 detector therefore uses a persistent NVS baseline:

- the very first boot after fresh/erased NVS has no `boot_seen`; it stores the marker and does NOT count that boot as a reset gesture;
- every later `POWERON` while `boot_seen` exists is indistinguishable in software from a physical RST/EN press and is accepted as a gesture step.

Thus, if the board had already completed the post-flash baseline boot before this video was recorded, then the later manual power application in the video is accepted as a reset step and produces WHITE #1. The subsequent physical RST/EN press produces WHITE #2.

This limitation was already documented in the Build #1821 hardware record: rapid power cycles can be indistinguishable from rapid RST presses because both are reported as `POWERON`.

## Current test status

- RGB driver is operational: WHITE is visibly produced.
- Build #1876 physical reset detector is active.
- This video does NOT validate a three-step sequence.
- This video does NOT test RED.
- Do not claim factory-reset completion from this video.
- Do not alter the RGB driver.

Next test must distinguish sequence behavior deliberately and count actual user actions, not video frames.
