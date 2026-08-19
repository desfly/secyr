# HomeGuard-S3 — GitHub build

Репозиторій проєкту охоронної системи та розумного дому на **ESP32-S3 HW-678 V0.0.0 / N16R8**.

## Що збирає GitHub Actions

Workflow:

```text
.github/workflows/homeguard-build.yml
```

Основні етапи:

1. **Host validation**
   - preflight;
   - аудит ESP-IDF API;
   - аудит залежностей;
   - контроль GPIO;
   - контроль таблиці розділів;
   - unit tests;
   - Web UI contract та browser smoke;
   - Android LAN discovery contract;
   - mock syntax;
   - mock compile/link.

2. **ESP-IDF 5.4.4 firmware**
   - `idf.py set-target esp32s3`;
   - `idf.py build`;
   - checksum і diagnostic artifacts;
   - firmware artifact після успішної збірки.

3. **Android debug APK**
   - unit tests;
   - debug APK build;
   - підпис і перевірка;
   - artifact `MyFist-Android`.

## Результати

При успішній збірці firmware:

```text
HomeGuard-S3-firmware
```

Діагностика:

```text
HomeGuard-S3-ESP-IDF-diagnostics
HomeGuard-S3-host-diagnostics
```

Android artifact:

```text
MyFist-Android
```
