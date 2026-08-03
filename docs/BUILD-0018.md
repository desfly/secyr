# HomeGuard-S3 Build-0018

Build-0018 moves from repeated domain-logic work to real HW-678
ESP-IDF integration.

## Implemented

- exact HW-678 / ESP32-S3-WROOM-1-N16R8 GPIO profile;
- compile-time reserved GPIO guard;
- ESP-IDF shared I2C master on GPIO4/GPIO5;
- real ADS1115 driver for addresses 0x48 and 0x49;
- real MCP23017 driver at address 0x20;
- MCP23017 Port A forced OFF during startup;
- MCP23017 Port B configured as pulled-up inputs;
- hardware bootstrap and module health states;
- portable REST response for `/api/v1/hardware/status`;
- Android hardware diagnostics models;
- host test of hardware health JSON.

## Startup order

1. Initialize NVS.
2. Force direct emergency outputs inactive.
3. Initialize I2C.
4. Initialize MCP23017.
5. Force MCP23017 Port A to zero.
6. Probe ADS1115 #1 and #2.
7. Start remaining drivers.
8. Start automation only after the safe-output state is confirmed.
9. Publish hardware health through REST/WSS.

## Not hidden

INA226, DS3231, W5500, microSD, 1-Wire and RS-485 are explicitly
reported as `not_initialized` until their Build-0019 drivers are attached.
They are not falsely reported as working.
