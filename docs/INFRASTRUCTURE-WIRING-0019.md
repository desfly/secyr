# Infrastructure wiring — Build-0019

## INA226

- SDA: GPIO4
- SCL: GPIO5
- address: 0x40
- logic voltage: 3.3 V
- default firmware shunt: 10 mΩ

The INA226 module must be wired according to the actual current direction
and installed shunt rating.

## DS3231

- SDA: GPIO4
- SCL: GPIO5
- address: 0x68
- INT/SQW: GPIO7
- backup cell required

## microSD adapter

- 3V3 → 3.3 V
- CS → GPIO39
- CLK → GPIO40
- MOSI → GPIO41
- MISO → GPIO42
- GND → GND

Only the small 3.3 V six-pin adapter is assumed.

## W5500

- RESET → GPIO8
- INT → GPIO9
- CS → GPIO10
- MOSI → GPIO11
- SCK → GPIO12
- MISO → GPIO13
- VCC → 3.3 V
- GND → GND
