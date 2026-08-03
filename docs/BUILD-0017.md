# HomeGuard-S3 Build-0017

Build-0017 continues from Build-0016 and focuses on physical sensors
and actuators.

## Supervised alarm loops

- digital low-pass filtering;
- configurable debounce by stable sample count;
- normal, alarm, short circuit, open circuit, unstable and sensor fault;
- transition counter;
- filtered and raw voltage in diagnostics.

## PT-506

- 120-ohm shunt by default;
- calibration gain and zero offset;
- 4–20 mA diagnostics;
- 0–10 bar conversion;
- exponential filtering;
- pressure rate calculation;
- open loop, underrange, overrange and electrical fault states.

## Water valves

- OPEN, CLOSED, OPENING, CLOSING;
- JAMMED and TIMEOUT;
- MANUAL mode;
- EMERGENCY_CLOSING;
- simultaneous end-switch fault detection;
- current-based jam detection;
- emergency latch preventing reopen;
- explicit manual emergency reset.

## DS18B20

- CRC/result validation;
- moving average;
- minimum and maximum;
- temperature change rate;
- invalid 85 °C startup reading rejection.

## Android

- hardware status models;
- zone dashboard;
- pressure dashboard;
- valve dashboard;
- temperature statistics presentation;
- portable formatting tests.
