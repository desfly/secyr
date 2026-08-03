# HomeGuard-S3 Build-0022

Build-0022 focuses entirely on the real build pipeline and firmware provenance.

## Added

- firmware version header;
- build metadata compiled into firmware;
- Git revision and UTC build timestamp;
- `GET /api/v1/build`;
- preflight source/CMake/partition/sdkconfig validator;
- duplicate source detection;
- missing `esp_check.h` detection;
- partition boundary validation for 16 MiB flash;
- firmware release manifest generator;
- release SHA256 generation;
- ESP-IDF 5.4.2 CI workflow with preflight dependency;
- Windows Build-0022 script;
- Android build-info model and formatter.

## Expected CI artifact

```text
HomeGuard-S3-Build-0022-firmware
```

Contents after a successful real build:

```text
bootloader.bin
partition-table.bin
homeguard_s3.bin
ota_data_initial.bin (when generated)
flasher_args.json
flash_args
SHA256SUMS.txt
manifest.json
```

## Current limitation

This archive still does not contain a genuine firmware binary because the
ESP-IDF build workflow has not been executed in this environment.
