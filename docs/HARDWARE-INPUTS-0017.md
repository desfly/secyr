# Hardware input map for Build-0017

## ADS1115 #1 — address 0x48

- A0: motion loop
- A1: entrance door loop
- A2: flood loop
- A3: smoke loop

## ADS1115 #2 — address 0x49

- A0: gas loop
- A1: cold-water PT-506
- A2: hot-water PT-506
- A3: reserve

## PT-506

- output: 4–20 mA
- range: 0–10 bar
- shunt: 120 Ω, 0.1%, at least 0.25 W
- 4 mA: 0.48 V
- 12 mA: 1.44 V
- 20 mA: 2.40 V

## DS18B20

- GPIO6
- external 4.7 kΩ pull-up
- three-wire powered mode
- no parasite power

## Valve protection

OPEN and CLOSE outputs must never be active simultaneously.
The motor current must be measured or protected by a dedicated fuse/current
limiter. Software timeout is not a replacement for electrical protection.
