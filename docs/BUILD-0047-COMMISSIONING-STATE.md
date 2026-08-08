# HomeGuard-S3 Build-0047 — commissioning state

Build-0047 makes hardware commissioning state explicit and reboot-safe at the data-model level.

The persistent state records schema version, GPIO-map verification, active-polarity verification, successful dry-run count, successful actuator-test count, and the last verification timestamp.

Validation is fail-closed. A state is rejected when the schema is unsupported, actuator-test count exceeds dry-run count, or GPIO/polarity verification is incomplete. Physical-output readiness additionally requires at least one successful dry run.

No GPIO number or active polarity is invented by this build. Those values still require physical verification on the target board before real actuator energization is allowed.
