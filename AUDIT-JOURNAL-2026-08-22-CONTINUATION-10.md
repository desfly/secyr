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

## Current technical implication

The reset/RGB path is not yet validated as a complete 3-step sequence in Build #1876.

The important new observation is that WHITE appears already during startup/power-up, even though the intended Build #1821 NVS baseline behavior was expected to establish `hg_rstseq/boot_seen` without counting the first boot as a gesture step. A single subsequent physical RST/EN press then produces the second WHITE.

Next debugging target: determine which code path requests the first startup WHITE. Do not alter the RGB driver. Do not claim factory-reset completion or RED behavior from this video.
