# HomeGuard-S3 Build-0028

Build-0028 integrates firmware, web and Android around one API contract.

## Firmware

- unified device-state model;
- state JSON serialization;
- command request and response model;
- 64-entry request-id replay protection;
- security arm/disarm commands;
- corridor-light command;
- two valve open/close commands;
- emergency latch blocking unsafe reopen;
- portable API tests.

## Web

- responsive dashboard;
- periodic state refresh;
- zones and 24/7 indication;
- temperatures and pressures;
- battery and mains state;
- arm/disarm controls;
- light controls;
- valve controls;
- firmware build information;
- PWA manifest.

## Android

- API interfaces and DTOs;
- repository with real product commands;
- main UI state;
- coroutine-based view model;
- command result handling;
- state refresh after commands;
- portable contract test.

No fake `.bin` or `.apk` is included.
