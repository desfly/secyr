# HomeGuard-S3 Build-0045 — Hardware Test Framework

Build-0045 adds a fail-closed hardware-test layer intended for bench commissioning.

## Included

- dry-run readiness report for controller telemetry, zones, analog channels, event log and physical outputs;
- explicit maintenance-mode gate;
- test rejection while the security system is armed;
- test rejection while an alarm is active;
- physical-output availability gate;
- bounded actuator pulse request: 1..1000 ms;
- target mapping for siren, valve 1, valve 2, AUX1 and AUX2;
- JSON serializers for readiness and test decisions;
- host safety tests covering every blocking gate and pulse limits.

## Current physical-output state

`SafeOutputs` intentionally keeps `available_ = false` because final GPIO numbers and active polarity have not yet been electrically verified. Build-0045 does **not** override this. Therefore actuator tests remain blocked on real hardware until the mapping is explicitly verified in a later hardware-commissioning step.

Dry-run diagnostics are available without energizing outputs.
