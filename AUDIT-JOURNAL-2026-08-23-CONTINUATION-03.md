# HomeGuard-S3 — audit journal — 2026-08-23 continuation 03

## Reference baseline before ADS1115 work

The user-confirmed reference build remains the firmware built from source commit `65f4b553740fec60ea4592671e3dfe911723a7c7`.

Confirmed on real hardware before this integration:

- Wi-Fi handover: PASS / OK;
- Web UI on PC: PASS / OK;
- Web UI on mobile: PASS / OK;
- physical reset indication: PASS / OK, sequence 3 white acknowledgements + 1 red completion indication.

These items are the regression baseline and must remain unchanged while integrating the two ADS1115 modules.

## Dual ADS1115 hardware contract

Approved shared I2C bus on HW-678 / ESP32-S3:

- SDA: GPIO4;
- SCL: GPIO5;
- supply: 3.3 V;
- common GND.

Module allocation:

1. ADS1115 #1 / zone ADC
   - I2C address: `0x48`;
   - ADDR strapped to GND;
   - channels A0..A3 exposed as raw millivolts until zone thresholds/functions are explicitly approved.

2. ADS1115 #2 / telemetry ADC
   - I2C address: `0x49`;
   - ADDR strapped to 3.3 V;
   - channels A0..A3 exposed as raw millivolts;
   - existing telemetry use of channels 0/1 is preserved; no new sensor semantics are invented for channels 2/3.

## Firmware changes

Final integration HEAD before this journal entry: `0fc34d852c1acfcfadd9393ea427cecdb4146e77`.

The integration now includes:

- physical ADS1115 detection by an actual config-register I2C transaction; adding a device handle alone is no longer accepted as READY;
- cleanup of a failed device candidate with `i2c_master_bus_rm_device()`;
- a per-ADS1115 mutex around the complete MUX/select -> conversion wait -> conversion read sequence, preventing telemetry and diagnostic HTTP requests from corrupting each other's channel selection;
- a four-channel `read_all_single_ended_mv()` API;
- boot-time A0 conversion self-test for both ADS1115 devices;
- explicit UART logs identifying ADS1115 #1 at 0x48 and ADS1115 #2 at 0x49 as detected/missing and showing the A0 self-test result when present;
- authenticated `GET /api/v1/hardware/analog` returning both devices and all 8 raw channels in millivolts;
- CI preflight contract locking GPIO4/GPIO5, addresses 0x48/0x49, physical probe, conversion mutex, four-channel API, and the diagnostic route.

## First hardware validation target

After a green firmware build, connect only the two ADS1115 modules to the approved I2C bus and capture UART.

Expected success evidence includes both lines in this form:

```text
ADS1115 #1 0x48 detected; A0 conversion self-test ... mV
ADS1115 #2 0x49 detected; A0 conversion self-test ... mV
```

The authenticated analog endpoint should then report two devices with addresses 72 (`0x48`) and 73 (`0x49`) and four `channels_mv` values for each.

Do not mark dual ADS1115 integration as hardware PASS until both physical addresses and conversion reads are observed on the real ESP32-S3.
