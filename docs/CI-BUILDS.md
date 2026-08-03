# Reproducible firmware and Android builds — Build-0013

Build-0013 separates portable validation from real firmware/APK compilation. Only successful full-toolchain jobs publish installable artifacts.

## Validation job

The `validate` job now performs:

1. portable C++ configure, build and tests;
2. 32 executable Kotlin protocol/queue checks;
3. focused resolver contract compilation;
4. compilation of every Android main-source Kotlin file against controlled platform/library stubs;
5. syntax compilation of every ESP-IDF adapter/application source;
6. one mock link of all ESP-IDF units against the portable core library;
7. source parity, security policy, certificate/SAN and secret-leak checks.

## GitHub Actions

Run **Build HomeGuard-S3 Build-0013** from Actions or push to `main`.

- `firmware` uses `espressif/idf:v5.4.4`, resolves pinned managed components, writes `dependencies.lock`, selects `esp32s3`, reconfigures and links the complete application.
- `android` uses JDK 17, Gradle 8.9 and Android API 35, then runs unit tests, lint and `assembleDebug`.

Expected artifact names:

```text
homeguard-s3-build0013-firmware
homeguard-s3-build0013-android
homeguard-build0013-validation
```

## Windows firmware build

Open an ESP-IDF v5.4.4 PowerShell terminal and run:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build_firmware.ps1
```

The script never flashes automatically.

## Windows Android build

Set `ANDROID_SDK_ROOT` or `ANDROID_HOME` to an SDK containing Platform 35 and Build Tools 34.0.0, ensure JDK 17 is available, then run:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build_android.ps1
```

When Gradle is absent, the script downloads Gradle 8.9 into `.tools`, verifies the pinned SHA-256, expands it and builds the debug APK. Use `-NoBootstrap` to require an existing Gradle installation.

## Safety boundary

A successful link proves toolchain/source compatibility only. It does not validate HW-678 GPIOs, voltage levels, output polarity, W5500 wiring or hazardous loads. Unresolved GPIO values stay `-1` until bench verification.
