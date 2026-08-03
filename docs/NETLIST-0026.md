# Logical netlist and connector plan

## J1 — 12 V input and battery

| Pin | Net | Description |
|---:|---|---|
| 1 | +12V_MAIN | regulated external 12 V supply |
| 2 | GND_POWER | power return |
| 3 | BAT_PLUS | protected 3S2P battery positive |
| 4 | BAT_MINUS | protected battery negative |
| 5 | POWER_FAIL | isolated/open-collector mains-loss signal |

## J2 — supervised zones

Every zone uses the agreed 3 kΩ end-of-line resistor. The exact voltage
thresholds are calibrated after assembly.

| Pin | Signal | ADS1115 |
|---:|---|---|
| 1 | ZONE_MOTION | ADC1 A0 |
| 2 | ZONE_DOOR | ADC1 A1 |
| 3 | ZONE_FLOOD | ADC1 A2 |
| 4 | ZONE_SMOKE | ADC1 A3 |
| 5 | ZONE_GAS | ADC2 A0 |
| 6 | ZONE_COM | protected sensor common |

Each input front end:

```text
field input
  -> replaceable fuse/PTC
  -> series resistor
  -> bidirectional TVS to protected common
  -> RC low-pass
  -> resistor divider/clamp limiting ADS1115 input to 0...3.3 V
```

## J3 — pressure transmitters

| Pin | Signal |
|---:|---|
| 1 | +12V_SENSOR |
| 2 | PT506_COLD_LOOP |
| 3 | PT506_HOT_LOOP |
| 4 | GND_SENSOR |

Each PT-506 channel:

```text
+12 V -> PT-506 -> precision 120 Ω shunt -> GND_SENSOR
                           |
                           +-> ADS1115 input through RC protection
```

Expected shunt voltages:

- 4 mA = 0.48 V
- 12 mA = 1.44 V
- 20 mA = 2.40 V

## J4/J5 — three-wire motorized valves

Each valve connector:

| Pin | Signal |
|---:|---|
| 1 | +12V_VALVE |
| 2 | DRIVE_OPEN |
| 3 | DRIVE_CLOSE |
| 4 | LIMIT_OPEN |
| 5 | LIMIT_CLOSED |
| 6 | GND_POWER |

The actual actuator may use a different wiring scheme. Confirm its datasheet
before energizing it.

## J6 — temperatures

| Pin | Signal |
|---:|---|
| 1 | 3V3 |
| 2 | ONEWIRE_GPIO6 |
| 3 | GND_LOGIC |

Use powered three-wire DS18B20 operation and a 4.7 kΩ pull-up.

## J7 — Ethernet W5500

| W5500 | HW-678 |
|---|---|
| RESET | GPIO8 |
| INT | GPIO9 |
| CS | GPIO10 |
| MOSI | GPIO11 |
| SCK | GPIO12 |
| MISO | GPIO13 |
| VCC | 3.3 V |
| GND | GND_LOGIC |

## J8 — microSD adapter

| microSD | HW-678 |
|---|---|
| CS | GPIO39 |
| SCK | GPIO40 |
| MOSI | GPIO41 |
| MISO | GPIO42 |
| VCC | 3.3 V |
| GND | GND_LOGIC |

## J9 — isolated RS-485

| Transceiver | HW-678 |
|---|---|
| DE/RE | GPIO16 |
| TX/DI | GPIO17 |
| RX/RO | GPIO18 |
| logic VCC | 3.3 V |
| isolated A/B | field bus |
