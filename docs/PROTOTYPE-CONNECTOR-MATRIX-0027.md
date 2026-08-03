# Prototype connector matrix

## Recommended detachable connectors

| Connector | Purpose | Minimum pins |
|---|---|---:|
| J1 | main 12 V, battery, power fail | 5 |
| J2 | five supervised zones + common | 6 |
| J3 | two PT-506 loops | 6 |
| J4 | cold-water valve | 6 |
| J5 | hot-water valve | 6 |
| J6 | two DS18B20 probes | 4 |
| J7 | isolated RS-485 A/B/shield | 3 |
| J8 | service UART | 3 |
| J9 | tamper/service inputs | 4 |

## Cable rules

- zone and 1-Wire cable: twisted pair, shield where runs are long;
- PT-506: shielded pair per loop;
- valve motor cable: separate from sensor and I2C wiring;
- Ethernet: standard twisted pair through W5500 RJ45;
- RS-485: 120 Ω twisted pair;
- never bundle 230 V conductors with SELV sensor wiring.
