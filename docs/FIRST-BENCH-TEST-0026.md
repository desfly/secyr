# First bench test sequence

## Stage 0 — no power

- inspect polarity and solder bridges;
- continuity test between 3.3 V and GND;
- continuity test between 12 V and GND;
- verify that 230 V section is physically isolated;
- disconnect valves, siren, lighting relay and battery.

## Stage 1 — power only

- current-limited laboratory supply, 12 V;
- limit current to approximately 0.3 A initially;
- verify 12 V, 5 V and 3.3 V rails;
- verify that no output relay energizes;
- check regulator temperatures.

## Stage 2 — ESP32-S3 only

- connect USB/UART;
- flash verified firmware;
- capture complete boot log;
- check reset reason, flash size and PSRAM;
- verify `/api/v1/build`.

## Stage 3 — I2C

Expected addresses:

```text
0x20 MCP23017
0x40 INA226
0x48 ADS1115 zones
0x49 ADS1115 telemetry
0x68 DS3231
```

Do not continue until the detected list matches installed modules.

## Stage 4 — LAN and storage

- connect W5500 without actuator power;
- obtain DHCP address;
- verify `/api/v1/hardware/status`;
- insert blank test microSD;
- verify mount and a test journal file.

## Stage 5 — passive sensors

- test each 3 kΩ loop using resistors at the connector;
- test DS18B20;
- simulate PT-506 with a current-loop calibrator or resistor/current source;
- confirm raw ADC values before accepting alarm thresholds.

## Stage 6 — outputs without loads

- observe driver outputs with a multimeter/LED test load;
- confirm all outputs OFF after reset;
- confirm OPEN and CLOSE never activate simultaneously.

## Stage 7 — one actuator at a time

- separate fused supply;
- current limit enabled;
- connect one valve;
- test STOP, OPEN, CLOSE and both end switches;
- repeat for second valve.

## Stage 8 — battery

- use protected 3S2P pack and correct charger/BMS;
- verify charge/discharge current sign;
- simulate mains loss;
- confirm journal and RTC continue operating.
