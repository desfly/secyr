# HomeGuard-S3 Build-0027

Build-0027 moves the hardware package from general documentation to
calculated prototype values.

## Added

- machine-readable hardware profile;
- supervised-zone divider calculations for 3 kΩ EOL;
- initial voltage windows for short, alarm, normal and open states;
- ADS1115 count table;
- complete PT-506 current/voltage/pressure table;
- calibration structures shared by firmware;
- validation of all zone and pressure calibration windows;
- portable calibration tests;
- prototype connector matrix;
- hardware configuration JSON schema for unresolved real parts.

## Still intentionally unresolved

A final valve driver schematic cannot be selected until the exact valve wiring
and motor current are known. The schema now records the required data so the
project does not silently assume the wrong driver.

A final 230 V measurement circuit is also blocked until the exact isolated
meter/module is selected.
