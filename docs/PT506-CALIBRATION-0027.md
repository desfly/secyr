# PT-506 4–20 mA, 0–10 bar calibration

## Electrical conversion

With a 120 Ω precision shunt:

```text
Vshunt = Iloop × 120 Ω
```

Therefore:

| Current | Voltage | Pressure |
|---:|---:|---:|
| 4 mA | 0.48 V | 0 bar |
| 8 mA | 0.96 V | 2.5 bar |
| 12 mA | 1.44 V | 5 bar |
| 16 mA | 1.92 V | 7.5 bar |
| 20 mA | 2.40 V | 10 bar |

Conversion:

```text
I_mA = V_mV / 120
Pressure_bar = clamp((I_mA - 4) / 16, 0, 1) × 10
```

## Diagnostic boundaries

| Current | Interpretation |
|---:|---|
| below 3.6 mA | broken loop / unpowered transmitter |
| 3.6–4.0 mA | underrange |
| 4.0–20.0 mA | valid |
| 20.0–21.0 mA | overrange |
| above 21.0 mA | electrical fault |

## Commissioning procedure

1. Apply a verified 4.000 mA loop current.
2. Record raw ADS1115 millivolts.
3. Apply 12.000 mA.
4. Apply 20.000 mA.
5. Calculate gain and zero correction separately for cold and hot channels.
6. Store calibration in NVS.
7. Repeat after final cable installation.
