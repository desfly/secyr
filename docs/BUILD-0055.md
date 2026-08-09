# HomeGuard-S3 Build-0055

## Scope

Build-0055 locks the agreed access model for the controller and its Web/Android clients.

- Maximum configured users: **8**.
- **Admin**: unrestricted command access.
- **User**: monitoring, arm home, arm away, disarm, open valves, close valves.
- **Guest**: sensor/state monitoring only; no control commands.

## Policy corrections

The previous User policy allowed `light.set` but did not allow `valve.open`. Build-0055 corrects that mismatch:

- added `valve.open` / `open_valves` to User permissions;
- retained `valve.close` / `close_valves`;
- removed `light.set` and `silence` from User permissions;
- kept `valve.clear_latch`, maintenance, configuration and other commands Admin-only.

## Enforcement

Permissions are enforced in firmware by `AccessControl::role_allows()`. The Web and Android UI must treat firmware authorization as authoritative rather than relying only on hidden or disabled buttons.

## Regression coverage

`tests/test_build0055.cpp` verifies:

- capacity remains exactly 8 users;
- Admin accepts representative unrestricted operations;
- User accepts security and valve commands;
- User rejects light, emergency-latch clearing and system reboot commands;
- Guest rejects all representative control operations.
