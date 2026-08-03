# Product readiness after Build-0028

## Implemented in source

- full portable device state contract;
- command router;
- duplicate request protection;
- security mode commands;
- corridor-light commands;
- water-valve commands and emergency latch;
- complete responsive web dashboard;
- Android API DTOs;
- Android repository;
- Android main state and view model;
- firmware and Android contract tests.

## Required to produce installable artifacts

### Firmware `.bin`

Run the project using the real ESP-IDF 5.4.2 toolchain. Source and host gates
are available, but a real binary cannot be truthfully generated without the
Espressif compiler and libraries.

### Android `.apk`

Run the Android directory with a compatible Android SDK/JDK/Gradle toolchain.
The current environment does not contain the Android SDK, so no genuine APK
has been produced.

## Hardware verification still required

- exact valve model and electrical wiring;
- actual valve current and driver selection;
- exact INA226 shunt;
- selected isolated 230 V meter;
- calibration of all five zones and both PT-506 channels;
- first boot on the real HW-678 board.
