# HomeGuard-S3 — hardware bench log — 2026-09-02

## Physical state at end of day

Five modules are physically connected to the ESP32-S3 and were tested together:

1. W5500 Ethernet
2. microSD SPI module
3. ADS1115 #1
4. ADS1115 #2
5. INA226 (CJMCU-226, shunt marked R100 = 0.100 ohm)

No soldering changes are to be made to INA226 until the full I2C address scan is completed.

## Canonical GPIO mapping used in the tests

### Shared I2C bus

- SDA = GPIO4
- SCL = GPIO5
- ADS1115 #1 = 0x48
- ADS1115 #2 = 0x49
- INA226 expected address = 0x40

### W5500

- RST = GPIO8
- INT = GPIO9
- CS = GPIO10
- MOSI = GPIO11
- SCK = GPIO12
- MISO = GPIO13

### microSD

- CS = GPIO39
- SCK = GPIO40
- MOSI = GPIO41
- MISO = GPIO42

## Build-1994 bench result

Build-1994 commit: `74008b8962253c6d7a4693990b11d5ffa342fefa`

Observed on real hardware:

- ADS1115 #1 0x48: PASS, A0 = 726.000 mV
- ADS1115 #2 0x49: PASS, A0 = 582.750 mV
- microSD: PASS — mounted, WRITE OK, READ OK, data verification OK
- W5500: PASS — SPI init OK, LINK UP, DHCP IPv4 = 192.168.55.252
- INA226: FAIL at this stage with `ESP_ERR_INVALID_STATE`

The INA226 code in Build-1994 was also corrected for the actual on-board shunt R100 = 0.100 ohm instead of the previous 0.010 ohm assumption.

## Build-1995 bench result

Build-1995 commit: `52b57200f6af68cb3d2bdab3131a743e910a7eca`

Build-1995 added a direct I2C probe before INA226 driver setup, identity reads, and detailed stage logging.

Observed on real hardware:

- I2C bus initialized: SDA GPIO4, SCL GPIO5, 400 kHz
- ADS1115 #1 0x48: PASS, A0 = 732.000 mV
- ADS1115 #2 0x49: PASS, A0 = 587.125 mV
- INA226 probe at 0x40: FAIL — `ESP_ERR_NOT_FOUND`
- microSD: PASS — mount + WRITE + READ + VERIFY
- W5500: PASS — LINK UP, IPv4 = 192.168.55.252

This narrows the INA226 problem significantly: the shared I2C bus and GPIO4/GPIO5 are working because both ADS1115 devices communicate and convert correctly. The INA226 itself does not ACK at address 0x40 in the current wiring/configuration.

## Current confirmed total

- ADS1115 #1: PASS
- ADS1115 #2: PASS
- microSD: PASS
- W5500: PASS
- INA226: NOT DETECTED AT 0x40

Current result: **4/5 modules confirmed working.**

## Next diagnostic step

Build-1996 commit: `621f2b6b71400bdd55611b231dbac9d3ed2b6f83`

A full I2C scan from 0x08 through 0x77 was added. If the INA226 probe at 0x40 fails, the firmware will print every address that ACKs on GPIO4/GPIO5.

Purpose of the next test:

- if another INA226-compatible address appears, correct the configured address in firmware;
- if only 0x48 and 0x49 appear, inspect INA226 power and wiring at the module itself: 3.3 V, GND, SDA, SCL;
- do not rework soldering before this scan result.

## Working PC folder convention

The user keeps reusing and cleaning this folder before extracting the next firmware artifact:

`C:\HomeGuard-S3-firmware-Build-1983`

The folder name therefore does not identify the actual firmware build. Always verify the build from the ESP boot log (`HomeGuard-S3 Build-XXXX` / app commit SHA).
