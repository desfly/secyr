# Supervised 3 kΩ zone front end — Build-0027

## Reference circuit per zone

```text
3.3 V
 |
 R_PULLUP = 3.00 kΩ, 1%
 |
 +-------> protected ADC node
 |
 field cable
 |
 sensor contact + 3.00 kΩ EOL resistor at the far end
 |
 GND_SENSOR
```

Recommended protection between the field terminal and ADC node:

```text
field terminal
  -> 1 kΩ series resistor
  -> 100 nF to GND_SENSOR
  -> low-capacitance 3.3 V TVS/clamp
  -> ADS1115 input
```

The exact arrangement depends on whether the detector contact is normally
closed or normally open. Build-0027 therefore fixes the electrical measurement
method but keeps the logical state mapping configurable.

## Reference voltages with 3.00 kΩ pull-up

For a simple divider:

```text
Vadc = 3.3 V × Rloop / (Rpullup + Rloop)
```

Typical points:

| Loop condition | Equivalent resistance | Expected Vadc |
|---|---:|---:|
| short circuit | 0 Ω | 0.00 V |
| low-resistance alarm/contact | 1.0 kΩ | 0.83 V |
| normal 3 kΩ EOL | 3.0 kΩ | 1.65 V |
| high-resistance alarm | 6.0 kΩ | 2.20 V |
| open circuit | infinite | 3.30 V |

## Initial software windows

These are commissioning defaults, not final field calibration:

| State | Voltage window |
|---|---|
| short | 0.00–0.35 V |
| low alarm | 0.55–1.10 V |
| normal | 1.25–1.95 V |
| high alarm | 2.00–2.55 V |
| unstable | 0.35–0.55 V, 1.10–1.25 V, 1.95–2.00 V, 2.55–2.95 V |
| open | 2.95–3.30 V |

Final thresholds must be calculated from actual cable resistance, sensor
contacts and measured 3.3 V rail.
