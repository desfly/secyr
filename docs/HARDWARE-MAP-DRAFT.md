# HW-678 hardware map — verified scope and unresolved GPIOs

## Confirmed from the supplied technical drawing

- Board revision: **HW-678 V0.0.0**.
- Controller module: **ESP32-S3-WROOM-1-N16R8**.
- On-board interfaces: USB-UART **CH343P**, USB-C UART, USB-C native USB, BOOT, RESET and WS2812 RGB LED.
- Ethernet: separate **W5500 LAN module** with RJ45 and ACT/LINK indicators.
- W5500 signal names: MOSI, MISO, SCLK, CS, INT and RST.
- Power blocks shown: 220 V AC input with F1 1 A, HLK-PM01 AC/DC, 3S2P 11.1 V battery with 3S BMS and XL4016 producing +5 V/GND.

## Not confirmed

The supplied drawing does not expose numeric ESP32-S3 GPIO assignments for I2C, W5500, alarm loops,
outputs or a dedicated service button. Build-0007 contained placeholder numbers; Build-0009 removes them.
No output or peripheral is enabled merely because a draft number existed in an earlier archive.

All pins default to `-1` in ESP-IDF Kconfig. `-1` means deliberately unassigned and disabled. The firmware
validates the configured map at boot and refuses startup when a GPIO is invalid, duplicated or when only
part of the I2C/W5500 bus is configured.

## Service/reset button behavior after its GPIO is verified

- Active-low/active-high is configurable.
- 40 ms default debounce.
- Hold for 3 seconds: enter maintenance mode; requested outputs are forced safe by the maintenance guard.
- Continue holding for 10 seconds: erase only provisioning credentials and reboot to Setup AP.
- Factory reset is rejected unless the same continuous hold already entered maintenance mode.

BOOT and RESET buttons shown on the ESP32 module are not automatically reused for this feature. A dedicated
GPIO must be confirmed electrically before `CONFIG_HOMEGUARD_SERVICE_BUTTON_GPIO` is changed from `-1`.

## Required physical verification

1. Trace every proposed GPIO from ESP32-S3 pad to the actual connector/component.
2. Check boot-strapping, USB, flash/PSRAM and board-reserved pins.
3. Confirm voltage levels, pull-ups/pull-downs and active polarity.
4. Verify W5500 INT and RST behavior and SPI chip-select exclusivity.
5. Confirm output driver polarity with loads disconnected.
6. Record the accepted map in a signed/revisioned schematic before enabling hazardous outputs.
