# HomeGuard-S3 Build-0021

Build-0021 is a buildability and firmware-release milestone.

No new alarm logic was added.

## Completed

- canonical ESP-IDF top-level CMake project;
- canonical `main/CMakeLists.txt`;
- complete `app_main.cpp`;
- NVS initialization and recovery;
- ESP-NETIF and default event loop initialization;
- hardware bootstrap startup;
- HTTP server startup;
- hardware status route registration;
- periodic telemetry task startup;
- ESP32-S3 N16R8 `sdkconfig.defaults`;
- 16 MiB production partition table;
- factory + ota_0 + ota_1;
- FAT storage partition;
- coredump partition;
- Windows environment checker;
- Windows clean/build/release script;
- Windows flash/monitor script;
- explicit dangerous erase script;
- pinned ESP-IDF 5.4.2 GitHub Actions workflow;
- release `.bin` and SHA256 artifact collection;
- repository structural validator.

## Current truth

The repository is now arranged for a real ESP-IDF build.

A firmware `.bin` has not been produced inside this environment because the
full ESP-IDF toolchain is not installed here. Build success remains unconfirmed
until either:

1. the GitHub Actions workflow succeeds, or
2. the Windows ESP-IDF build script completes successfully.

Do not flash a fabricated or unverified binary.
