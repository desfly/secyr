# HomeGuard-S3 — session resume, 2026-08-21 evening

## Rule for this and following sessions

Do not re-invent already completed work. Before changing code, read the latest commits and the session/build notes. Record every material diagnosis, decision, code change, validation result, and remaining blocker in Git so the next session has an explicit restart point.

## Verified baseline

Resume from `main` commit `b83157e60311740be9db1e0797642511876b3e39`.

Latest relevant commits already present in Git:

- `b83157e` — restores the physical RST/EN factory-reset sequence and locks the RGB feedback contract. Accepted physical reset steps are acknowledged with WHITE; successful destructive reset is confirmed with RED for 5 s; CI contains a guard against regression to GPIO21.
- `a385198` — stabilizes Wi-Fi handover and HTTP socket lifecycle (Build-0027 network/runtime work).
- `17a9940` — removes Wi-Fi transport masking and streams Web assets.
- `651a4aa` — rewrites Wi-Fi runtime as an asynchronous transactional state machine.

The Web tree currently contains the production dashboard assets (`index.html`, `app.css`, `app.js`, access/session code, factory-reset code, manifest and Bruce asset). Build-0028 documents the unified firmware/Web/Android API contract and real valve/light/security commands.

## Work already treated as implemented, not to be re-created

1. Web/HTTP transport and socket-lifecycle stabilization.
2. Wi-Fi candidate/commit handover logic.
3. Physical RST/EN factory-reset detection.
4. RGB reset feedback contract implemented in the latest commit.
5. Unified API model for firmware, Web and Android from Build-0028.

## What must still be proven

The latest code state is not considered a test release merely because the source changes exist. The remaining gate is validation:

- verify CI/build outcome for the current head;
- inspect any failing workflow/job rather than creating another speculative fix;
- only after a green build, identify the produced firmware artifact intended for ESP32-S3 testing;
- then perform hardware validation of the RST/RGB behavior and continue PC Web UI testing from the last tested point; mobile Web UI remains a later test stage.

## Hardware direction remembered from 2026-08-21 discussion

- Do not buy/use the MCP2515 + TJA1050 board for the current valve-control direction simply because it is a CAN module; it is not the chosen answer for the discussed LIN-style valve link.
- Valve-driver selection remains tied to the actual 12 V valve wiring/current once the actuators are opened/inspected.
- Optional cellular/SMS fallback remains a separate future hardware item and is not allowed to destabilize the current firmware/Web validation path.

## Immediate next action

Continue from `b83157e`: determine the real build/CI state of this exact code, fix only demonstrated failures, and append the result here (or in the next build note) before moving to hardware flashing/testing.
