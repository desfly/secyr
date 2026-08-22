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

## Immediate next action

1. Verify the latest `HomeGuard-S3 Build` run on the current `main` head.
2. If the RST/RGB gate is green, continue from hardware/runtime validation rather than reopening the old 19-Aug auth checklist.
3. If the gate fails, inspect the failing job/step and fix it directly, then record the fix here.

## Resume rule for the next session

1. Read this file first.
2. Read the newest commits after the resume base above before assuming any item is still open.
3. Check the latest `HomeGuard-S3 Build` status.
4. If the RST/RGB CI gate is green, continue from hardware/runtime validation rather than reopening the old 19-Aug auth checklist.
5. Record every material fix and the reason for it here or in the next continuation file before ending the session.
