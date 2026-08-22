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

There is a CI integration gap at this exact recovery point.

`tools/check_reset_rgb_contract.py` exists and enforces the physical RST/RGB contract, but `.github/workflows/homeguard-build.yml` does not invoke it in `host-gates`.

This means the latest commit message says the contract is gated against regression, but the main HomeGuard build does not currently execute that dedicated gate. Host unit tests cover the pure reset-sequence helper, but they do not replace the source-level contract check that forbids regression to GPIO21/service-button logic and verifies staging/LED ordering.

## Immediate next action

Add `python tools/check_reset_rgb_contract.py` to the main `Host validation` job, adjacent to factory-reset coverage, then verify the resulting GitHub Actions run.

## Resume rule for the next session

1. Read this file first.
2. Read the newest commits after the resume base above before assuming any item is still open.
3. Check the latest `HomeGuard-S3 Build` status.
4. If the RST/RGB CI gate is green, continue from hardware/runtime validation rather than reopening the old 19-Aug auth checklist.
5. Record every material fix and the reason for it here or in the next continuation file before ending the session.
