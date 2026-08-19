# HomeGuard-S3 access lifecycle security contract

This document is a normative product/security contract. Firmware, Web UI and Android must not weaken it.

## States

### FACTORY / SETUP_REQUIRED

Entered only when no persisted access user database exists, normally after a physical factory reset.

- Local first access is passwordless only for initial commissioning.
- The controller must expose no historical/user/private system state inherited from a previous installation.
- Initial commissioning must lead to creation of the first enabled Admin and a valid credential.
- Bootstrap is never a normal login path and cannot create a second first Admin.

### LOCKED / LOGIN_REQUIRED

Entered immediately after the first Admin has been persisted, and on every later boot while at least one valid access database exists.

- Unauthenticated Web UI shows only full-screen Bruce artwork plus username and password/PIN fields and a Login action.
- No dashboard, zones, events, network identifiers, IP addresses, cloud state, outputs, menus, build details or user information may be visible before authentication.
- Android follows the same rule: a discovered controller may be identified as requiring login, but protected system state is not displayed before authentication.
- Logout returns immediately to the locked Bruce + login view.

### AUTHENTICATED

Entered only after backend credential verification succeeds.

- UI content and actions are limited by the authenticated role/capabilities.
- Backend authorization remains authoritative; hiding a control in Web/Android is not a security boundary.
- Direct calls to protected APIs without valid authorization must fail closed.

## Bootstrap invariants

1. `SETUP_REQUIRED -> LOCKED` occurs only after the first enabled Admin is successfully persisted.
2. A normal reboot, browser refresh, new PC, new phone, app reinstall, Wi-Fi reconnect or power cycle must never reopen bootstrap.
3. Only the approved physical factory-reset gesture may erase the persisted access database and make bootstrap available again.
4. If persistence of the first Admin fails, bootstrap remains available and no half-created Admin is accepted.
5. If an access database exists but is unreadable/corrupt, the system fails closed; it must not silently fall back to passwordless bootstrap.
6. There must always remain at least one enabled Admin after normal user-management operations.

## Public unauthenticated surface

After bootstrap is complete, an unauthenticated client may receive only the minimum information necessary to choose the authentication screen, e.g. `login_required`. It must not receive protected controller state.

During factory setup, a client may receive only the minimum information needed to complete commissioning, e.g. `setup_required`, plus explicitly approved setup capabilities.

## Visual contract

Locked screen:

- Bruce fills the background/screen.
- One compact authentication panel only.
- Fields: user name/ID and password/PIN.
- Action: Login.
- No navigation, status cards, system data or ghosted dashboard behind it.

This contract applies equally to desktop Web UI and Android.
