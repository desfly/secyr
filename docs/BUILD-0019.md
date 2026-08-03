# HomeGuard-S3 Build-0019

Build-0019 continues the real HW-678 integration.

## Real ESP-IDF drivers added

- INA226 at I2C address 0x40;
- DS3231 at I2C address 0x68;
- microSD through SDSPI on GPIO39–GPIO42;
- W5500 Ethernet on SPI2, GPIO8–GPIO13.

## INA226

Default bootstrap calibration:

- shunt: 10 mΩ;
- maximum current: 20 A;
- bus voltage;
- signed charge/discharge current;
- power;
- shunt voltage.

The actual shunt value must match the installed hardware.

## DS3231

- read RTC time;
- write synchronized time;
- read internal RTC temperature;
- BCD conversion;
- no fabricated system time when RTC access fails.

## microSD

- SPI3 host;
- CS GPIO39;
- CLK GPIO40;
- MOSI GPIO41;
- MISO GPIO42;
- FAT mount at `/sdcard`;
- no automatic formatting after mount failure;
- total/free space query.

## W5500

- SPI2 host;
- 20 MHz initial SPI frequency;
- DHCP through ESP-NETIF;
- link state;
- assigned IPv4 state;
- reset and interrupt pins from the exact HW-678 profile.

## Remaining hardware layer

Build-0020 will connect:

- 1-Wire/DS18B20 to the ESP-IDF runtime;
- isolated RS-485 meter;
- real HTTP hardware/infrastructure endpoints;
- periodic telemetry task;
- first complete ESP-IDF CI build.
