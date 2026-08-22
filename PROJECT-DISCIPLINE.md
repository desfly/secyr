# HomeGuard-S3 — Project Discipline Rules

These rules are mandatory for all future work on the project.

1. **No claim of FIXED without hardware proof.**
   A code change, green CI, or passing source-level test is only `CODE READY`. A bug becomes `FIXED` only after the flashed ESP32-S3 shows the expected result on real hardware and the user confirms it.

2. **One bug = one traceable chain.**
   Every bug must have: symptom -> source cause -> commit SHA -> CI result -> flashed build/run -> hardware result. Never mix several unrelated fixes into one conclusion.

3. **Source is not binary.**
   Before asking the user to flash, verify which commit SHA produced the firmware artifact. After flashing, compare the device build info with that SHA. Do not assume `main` equals the binary on the device.

4. **UI tests must test behavior, not strings.**
   A grep/contract test that only finds code text does not prove the UI works. For dynamic UI controls, tests must verify DOM insertion/visibility/click behavior where practical. Final acceptance is still real-browser hardware validation.

5. **No guessing or substitution.**
   For hardware, UI state, build state, pinout, component identity, or test result: if not observed or verified, mark it `UNKNOWN` / `NOT VERIFIED`. Never replace the exact object with a similar one.

6. **Do not move to the next test after a FAIL.**
   During the Web UI marathon, each failed checkpoint is fixed, rebuilt, flashed, and re-tested before proceeding unless the user explicitly says to defer it.

7. **Record failures, not only successes.**
   Audit notes must include what was wrong, why the previous assumption failed, and what safeguard was added so the same class of mistake is harder to repeat.

8. **Status vocabulary is strict.**
   - `FOUND` — symptom reproduced/observed.
   - `CAUSE FOUND` — root cause identified.
   - `CODE READY` — fix committed.
   - `CI PASS` — automated checks passed.
   - `FLASHED` — exact artifact flashed.
   - `HW PASS` — user/hardware confirms correct behavior.
   - `FIXED` — only after `HW PASS`.

These rules exist because repeated premature declarations and substitutions wasted test time. They are part of the project process, not optional guidance.
