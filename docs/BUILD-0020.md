# HomeGuard-S3 Build-0020

Build-0020 completes the first pass of the real ESP-IDF hardware runtime.

## Added

- GPIO-based 1-Wire runtime on GPIO6;
- DS18B20 SEARCH ROM;
- CRC verification;
- conversion and scratchpad reading;
- support for up to eight DS18B20 devices;
- ESP-IDF UART1 RS-485 half-duplex driver;
- Modbus RTU CRC16;
- request/response transaction API;
- periodic infrastructure telemetry task;
- HTTP `GET /api/v1/hardware/status`;
- first full ESP-IDF GitHub Actions workflow using ESP-IDF 5.4.2;
- Android runtime diagnostics for 1-Wire and RS-485.

## RS-485 electrical requirement

The ESP32 pins must not connect directly to an RS-485 line.
Use an isolated 3.3 V transceiver module with:

- TX → GPIO17;
- RX → GPIO18;
- DE/RE → GPIO16;
- isolated A/B terminals;
- appropriate termination and bias;
- protected power supply.

## Build gate

The repository now contains a real ESP-IDF CI job. A successful CI build
is required before firmware is called compilable. Host tests alone do not
prove ESP-IDF API compatibility.
