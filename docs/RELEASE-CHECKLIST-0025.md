# Build-0025 release checklist

Before flashing:

- all seven host gates PASS;
- real ESP-IDF 5.4.2 build PASS;
- `homeguard_s3.bin` exists;
- `manifest.json` exists;
- `SHA256SUMS.txt` matches all files;
- firmware size fits the factory/OTA partition;
- bootloader and partition table are from the same CI run;
- board is confirmed as ESP32-S3-WROOM-1-N16R8;
- external 12 V loads are disconnected for first boot;
- valve motors are disconnected for first boot;
- only USB/UART, LAN and low-voltage sensors are tested first.

A mock-built host executable must never be flashed to the ESP32-S3.
