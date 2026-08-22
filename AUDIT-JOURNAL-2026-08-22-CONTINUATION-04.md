# HomeGuard-S3 audit journal — continuation 04

Date: 2026-08-22 (Europe/Kyiv)
Repository: `desfly/secyr`
Resume base: `b83157e60311740be9db1e0797642511876b3e39`

## Why this continuation exists

The previous formal recovery point (`AUDIT-JOURNAL-2026-08-19-CONTINUATION-03.md`) is stale relative to `main`. Work continued substantially on 2026-08-19 through 2026-08-21, so its old resume checklist must not be treated as the current backlog without checking commits first.

## Reconciled state since continuation 03

The previously open authentication/security migration was completed by later commits: Bearer-session authorization became the primary Web/API model; output/system/Wi-Fi/cloud/service/factory-reset boundaries were migrated; telemetry session handling and credential scrubbing were hardened; Android and ESP-IDF access paths were audited/migrated.

Work then advanced through Web UI, Wi-Fi runtime/stability, and physical reset/RGB behavior.

Current `main` head at resume time is:

`b83157e60311740be9db1e0797642511876b3e39` — **Restore physical RST/EN factory reset and lock RGB contract**.

Current physical reset contract encoded in source/tests:

- use the real physical RST/EN reset path, not GPIO21/service-button substitution;
- true cold power-on must not count as a reset gesture step;
- three accepted physical RST steps are required;
- each accepted step is acknowledged by WHITE RGB for 1500 ms;
- sequence state is persisted across the hardware reset and expires after a 5000 ms window;
- destructive factory-reset work is staged and performed on a safe early boot;
- a successful erase is acknowledged RED for 5000 ms, then RGB OFF and reboot.

Relevant protections already present in the tree:

- `tests/test_reset_sequence.cpp`
- `tools/check_reset_rgb_contract.py`
- `firmware/esp-idf/main/hg_reset_sequence.cpp`
- `firmware/include/homeguard/reset_sequence.hpp`

## Finding on resume

There was a CI integration gap at this exact recovery point.

`tools/check_reset_rgb_contract.py` existed and enforced the physical RST/RGB contract, but `.github/workflows/homeguard-build.yml` did not invoke it in `host-gates`.

This meant the latest reset commit said the contract was gated against regression, but the main HomeGuard build did not actually execute that dedicated gate. Host unit tests covered the pure reset-sequence helper, but they did not replace the source-level contract check that forbids regression to GPIO21/service-button logic and verifies staging/LED ordering.

## Action completed on 2026-08-22

Added a dedicated `Physical RST/RGB reset contract` step to the main `HomeGuard-S3 Build` / `Host validation` job:

`python tools/check_reset_rgb_contract.py`

Implementation commit:

`a84f03a8ef78c9406265802fc2b16a00065b215d` — **ci: enforce physical RST RGB reset contract**

The gate now runs immediately after `Factory reset coverage audit` and before the firmware-budget and host-unit-test stages.

A direct local clone/run from the assistant execution container was not possible because that container had no DNS/network path to GitHub. This is not treated as test success; the authoritative result remains the GitHub Actions run for the current `main` head.

## Hardware/runtime validation — point 1 (2026-08-22)

User tested the currently flashed ESP32-S3 on real hardware.

Observed:

- on power-up the RGB LED lights WHITE for approximately 2 seconds;
- first-boot/setup Web UI is reachable through the ESP AP address `192.168.4.1`;
- the same setup Web UI is also reachable through the infrastructure/STA address `192.168.55.253`;
- Bruce background/image renders correctly on both paths;
- the first comparison showed different apparent setup-card/font sizes between the two IPs;
- after resetting Chrome zoom independently for both origins with `Ctrl+0` and forcing reload with `Ctrl+F5`, the setup-card geometry, fonts, inputs, buttons and Bruce layout matched visually.

Conclusion for point 1:

**PASS** for dual-path Web UI reachability/rendering (`192.168.4.1` + `192.168.55.253`). The apparent size mismatch was browser per-origin zoom state, not different firmware Web UI assets.

Note: the two final screenshots had slightly different overall screenshot/window heights, but the Web UI geometry itself matched after zoom reset.

## Hardware/runtime validation — point 2 (2026-08-22)

User executed the physical three-step RST/EN reset sequence on the real ESP32-S3 and supplied video `4378.mp4` plus post-reset screenshots.

Observed:

- each of the three short physical RST/EN presses produced a WHITE acknowledgement after reboot;
- after the third accepted press the device entered the staged destructive-reset path and rebooted automatically;
- a final approximately five-second RGB indication was visible before the clean reboot;
- post-reset `192.168.55.253` timed out, confirming the previously stored infrastructure Wi-Fi configuration was erased;
- the ESP AP `HomeGuard-S3-A711` remained available and `192.168.4.1` showed the clean `Первинне налаштування` first-boot screen again;
- during the automatic reboot sequence Windows briefly attached to the ESP AP, lost it while the ESP rebooted, and the user then manually reconnected. This is consistent with the expected AP interruption across restart.

Conclusion for point 2:

**PASS for the physical RST/EN gesture and destructive factory-reset function.** The three-step sequence is recognized, reset state is erased, infrastructure Wi-Fi is removed, AP provisioning returns, and first-Admin setup is restored.

One RGB defect remains open: the final approximately five-second success indication appears WHITE in the supplied video, while the contract/source requires RED for five seconds. Do not reopen the reset detection or factory-reset logic for this symptom; isolate the RGB success-color path specifically.

## RED RGB root cause isolation and implementation fix (2026-08-22)

The exact firmware actually flashed during the hardware test was recovered from the 2026-08-21 UART/flash log:

- HomeGuard-S3 Build-1813;
- app commit `22d9f1e804b33d890deca54fdb38595d171ea0ac`;
- compile time 2026-08-21 17:15;
- this build already contains the `b83157e6...` physical RST/RGB reset base.

Therefore the WHITE final indication was not caused by testing an old pre-RED firmware.

Inspection of that exact flashed commit confirmed:

- onboard RGB is driven as one WS2812 on GPIO48;
- WHITE frame is `FF FF FF`;
- RED frame is GRB `00 FF 00`;
- the successful factory-reset path explicitly calls `set_red()` and blocks for five seconds before OFF/reboot;
- no later boot task can overwrite the LED during that early-boot five-second confirmation;
- mutable-state erase itself does not reboot before RED.

The concrete electrical/protocol mismatch was in the custom WS2812 RMT bit timing. At 10 MHz the previous driver used:

- logical 0: 0.3 us HIGH + 0.9 us LOW (`3/9` ticks);
- logical 1: 0.9 us HIGH + 0.3 us LOW (`9/3` ticks).

Espressif's conservative WS2812 reference timing at the same 10 MHz is:

- logical 0: 0.4 us HIGH + 0.8 us LOW (`4/8` ticks);
- logical 1: 0.8 us HIGH + 0.4 us LOW (`8/4` ticks).

This distinction is material for a mixed-color frame such as RED (`00 FF 00`), while all-zero/all-one frames such as OFF/WHITE can still appear correct with marginal timings.

Implementation commit:

`0d743156fb367168a745d5bd19dc9cf343f2f7f2` — **fix: align onboard WS2812 timing with Espressif reference**

Changes in that commit:

- `firmware/esp-idf/main/hg_rgb_diagnostic.cpp`: changed only WS2812 bit timing from `3/9, 9/3` to `4/8, 8/4`; GPIO48, GRB byte order, RED frame, reset sequence and 80 us latch are unchanged;
- `tools/check_reset_rgb_contract.py`: now locks the 10 MHz `4/8, 8/4` timing, 80 us latch and the RED/WHITE byte frames, and rejects regression to the obsolete `3/9, 9/3` timing.

The physical three-step RST/EN logic is intentionally untouched because point 2 already proved it on hardware.

## Immediate next action

1. Obtain the firmware artifact built from `0d743156fb367168a745d5bd19dc9cf343f2f7f2` or later main head containing that commit.
2. Flash that new artifact; do not reuse Build-1813 for the RED retest.
3. Retest only the physical three-step RST/EN sequence and verify the final success indication is RED for approximately five seconds.
4. If RED passes, close the RGB defect and continue remaining PC Web UI flow, then mobile Web UI.
5. Record the new build/run/SHA and hardware result here before moving on.

## Resume rule for the next session

1. Read this file first.
2. Read the newest commits after the resume base above before assuming any item is still open.
3. Check the latest `HomeGuard-S3 Build` status.
4. Treat point 1 Web UI and point 2 physical factory reset as hardware-validated; do not reopen them unless a regression appears.
5. Continue from the RED retest using a build containing `0d743156...`, then PC/mobile Web UI validation.
