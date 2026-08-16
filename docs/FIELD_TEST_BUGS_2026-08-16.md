# HomeGuard-S3 field-test defect contract — 2026-08-16

Source baseline: Build-877 / PR #47 head before hardening.

These findings are cemented. A later revision must not regress any item that is fixed here.

## Hardening implementation status

The branch `agent/hardware-runtime-hardening-20260816` now contains source-level fixes for the defects below. **Implemented does not mean field-validated.** No replacement firmware/APK build has been requested or launched from this hardening pass yet, so every item remains subject to compile/CI and then clean hardware/phone field validation before it can be called verified.

Implemented in source on the hardening branch:

- event-driven Wi-Fi recovery with recovery-AP restoration, bounded one-shot reconnect backoff and side-effect-free `GET /api/v1/network/status`;
- partial Wi-Fi startup rollback using the matching ESP-IDF default-netif destroy API;
- W5500 GPIO ISR ordering plus reverse-order, ownership-safe cleanup;
- physical I2C probing and transactional lifecycle for ADS1115, MCP23017, INA226 and DS3231;
- telemetry gating for failed/missing optional hardware and rate-limited 1-Wire rediscovery;
- graceful microSD failed-mount cleanup and no periodic refresh while unmounted;
- degraded hardware-bootstrap reporting and normal ESP-IDF logging path;
- authenticated full Factory Reset endpoint that erases the default NVS partition and restarts;
- compact LAN and CLOUD state indicators on normal Android device cards;
- CI/preflight source contracts intended to prevent these fixes from silently regressing.

## Historical reconstruction: Wi-Fi state vs Android LAN discovery

Three different operations must never be conflated again:

1. **ESP Wi-Fi network scan** — `/api/v1/network/scan`, which searches nearby Wi-Fi SSIDs for Wi-Fi setup.
2. **Android LAN discovery** — mDNS / UDP / bounded HTTP discovery used to find HomeGuard controllers already reachable on the phone's current LAN/Wi-Fi.
3. **Firmware update state persistence** — ordinary firmware flashing that leaves NVS intact, so previously stored Wi-Fi credentials/access state survive across builds.

The 2026-08-16 failure sequence did **not** include an explicit ESP Wi-Fi SSID scan. Therefore `/api/v1/network/scan` is not accepted as the diagnosed cause of today's Wi-Fi dropout. Its non-destructive handling remains preventive hardening only.

### Proven discovery timeline

- **Run 835** — branch `rescue/mainline-restore-working-discovery-20260815`, SHA `f811af5e9f3222b1ff2f78e54eebdcd6c47c7757`, title `TEST-A: restore known-working Android LAN discovery`. This is the earliest strong field control point in this sequence: the Android artifact was field-tested and discovery was explicitly reported working even though the overall CI run had another failing gate.
- **Run 836** — SHA `655d5f49a2d874028e26151b7f6dbe3b7d526db1`, commit `fix(android): use network status fallback across local subnet`. HTTP discovery changed so `/api/v1/network/status` could be used as a fallback across the whole local /24, not just the setup subnet. At this exact revision firmware `GET /api/v1/network/status` was still read-only.
- **Run 859** — SHA `e78e4d7aeac863d18a48f3d65a6c619847a41166`, commit `firmware: retire setup AP after STA gets an IP`. From this point firmware `GET /api/v1/network/status` gained a side effect: after returning status it could switch Wi-Fi from APSTA to STA in order to retire the recovery/setup AP.
- **Run 872** — SHA `36db0bd51654eaf1229b840f991c8edf172f16bb`, successful CI. Android combined mDNS + UDP + HTTP. A real `HG-*` identity and a fallback `setup-IP-80` identity could still be grouped separately, so one physical controller could appear more than once.
- **Run 877** — SHA `067ba5392315ff6f579b460f9f0a73a005774e8e`, successful CI. The discovery engine was effectively unchanged from Run 872. Field testing showed LAN/Wi-Fi discovery could still add the controller, but duplicate cards were not reliably collapsed and the requested normal-list state indicators were absent.
- **After Run 877** — commits `e1c05ef3` (`canonicalize physical HomeGuard identity`), `f166d2cd` (`merge duplicate physical controllers`) and `7d976a37` (`dedupe discovery by physical controller identity`) added the cross-transport physical identity model now present at PR #47 head.

### Consequences of that history

- **Run 835, not Run 872, is the field-proven discovery baseline** for regression reasoning.
- Run 877 is also proof that discovery was not completely dead: it found/added a controller; its observed defects were downstream identity/UI issues.
- Persisted Wi-Fi credentials moving from build to build are expected during ordinary update flashing when NVS is not erased. That behavior must not be confused with a clean-state test.
- Android LAN discovery and ESP Wi-Fi SSID scanning are different functions.
- A status endpoint must be observational. Coupling `GET /network/status` to Wi-Fi mode changes was a design defect because Android discovery can call that endpoint automatically.
- The status-side-effect alone is not proof that STA itself dropped: switching APSTA to STA normally retires AP while retaining STA. The separate absence of a real STA reconnect state machine remains the confirmed recovery weakness if STA drops for any reason.

## 1. Wi-Fi recovery after disconnect — P0

**Observed:** STA can drop and remain unreachable after the recovery AP has already been retired.

**Confirmed source defects:**

- no `WIFI_EVENT_STA_DISCONNECTED` reconnect state machine;
- recovery AP retirement was coupled to normal HTTP/status flow instead of Wi-Fi/IP events;
- Android LAN discovery could call that status endpoint automatically after Run 836.

**Not a diagnosed cause of today's observation:** explicit ESP Wi-Fi SSID scan. No such scan was performed in the relevant field sequence.

**Required behavior:** automatically reconnect after a real STA loss, restore the recovery AP while disconnected, retire it only after `IP_EVENT_STA_GOT_IP`, and keep status GET side-effect free. Wi-Fi SSID scan must remain non-destructive as a separate hardening rule.

**Hardening branch:** implemented with event handlers and non-blocking bounded reconnect backoff; pending compile/field validation.

## 2. W5500 failed-init resource leak — P0

**Observed:** repeated W5500 command timeouts continue after bootstrap already reported Ethernet failure.

**Root cause:** partially created MAC/PHY/driver/netif resources survived failure paths.

**Required behavior:** every failed init/start path performs full reverse-order rollback; no W5500 SPI activity is allowed after the driver is classified unavailable.

**Hardening branch:** ownership-safe rollback implemented; pending field validation with W5500 absent/failing.

## 3. W5500 GPIO ISR service ordering — P0

**Observed:** `GPIO isr service is not installed` during W5500 initialization.

**Required behavior:** install the GPIO ISR service before the W5500 interrupt-backed MAC is created; `ESP_ERR_INVALID_STATE` means the shared service already exists and is acceptable.

**Hardening branch:** implemented; pending field validation.

## 4. INA226 false Ready state and log flood — P0

**Observed:** `ina226: bus voltage` error repeats once per telemetry cycle even when the device did not initialize successfully.

**Root cause:** a non-null I2C handle was treated as Ready after later configuration writes failed.

**Required behavior:** failed configuration removes the device handle, Ready requires completed initialization, and ordinary runtime read failure must not emit one ESP_ERROR per second.

**Hardening branch:** implemented together with runtime gating; pending field validation with INA226 absent.

## 5. I2C phantom devices — P0

**Observed:** devices can be shown Ready even though physical presence was never verified.

**Root cause:** `i2c_master_bus_add_device()` only creates a software handle; it is not a physical ACK probe.

**Required behavior:** every I2C device must pass `i2c_master_probe()` before a runtime handle is accepted.

**Hardening branch:** implemented; ADS1115, MCP23017, INA226 and DS3231 additionally require successful device/register initialization before Ready.

## 6. MCP23017 partial initialization — P0/P1

**Observed risk:** MCP23017 can retain a handle after one of its configuration writes fails.

**Required behavior:** initialization is transactional; any failed direction/pull-up/safe-output write removes the handle and leaves the module unavailable. Outputs remain fail-closed.

**Hardening branch:** implemented; pending field validation.

## 7. Misleading hardware-bootstrap success — P1

**Observed:** log can say `Hardware bootstrap completed` while multiple optional modules failed.

**Required behavior:** distinguish Ready, Missing/Degraded and Fault. The boot log must explicitly say when bootstrap completed in degraded mode and report the number of unavailable/degraded optional modules.

**Hardening branch:** implemented; pending log validation on the next hardware run.

## 8. microSD failure cleanup — P1

**Observed:** absent/bad microSD produces a low-level init failure during bootstrap.

**Required behavior:** absence is a controlled unavailable state; a failed mount must clear card state and release an SPI bus owned by the SD runtime. Telemetry must not periodically call storage refresh when no card is mounted.

**Hardening branch:** implemented; pending field validation with no card installed.

## 9. Runtime must respect bootstrap state — P1

**Observed:** telemetry can continue polling hardware that bootstrap already classified Missing/Fault.

**Required behavior:** runtime sampling is gated by hardware-module state. Missing/Fault RTC, ADC, INA226, SD and other optional hardware must not be continuously polled.

**Hardening branch:** implemented; absent 1-Wire sensors are additionally rediscovered only on a bounded periodic cadence instead of once per telemetry second.

## 10. Clean-state test / Factory Reset invariant — TEST RULE

**Observed:** Build-877 restored Wi-Fi and one access user because an ordinary firmware `write-flash` did not erase mutable NVS.

This is not itself a Build-877 defect. It is a mandatory test rule:

- ordinary update must preserve state when expected;
- a true Factory Reset/full clean test must erase mutable user-owned state;
- immutable device identity and installed firmware must remain intact;
- the first-Admin bootstrap must become available again after a true Factory Reset.

A firmware test may not be called `from zero` if old Wi-Fi/access state is still present.

**Hardening branch:** authenticated `POST /api/v1/service/factory-reset` with explicit `ERASE_ALL` confirmation now erases the full default NVS partition and restarts. This is implemented but must not be called verified until a real reset proves Wi-Fi/access/cloud/provisioning state is gone and immutable identity remains.

## 11. Interleaved UART logging — P2

**Observed:** output from multiple tasks/components becomes mixed into hard-to-read lines.

**Root cause:** application replaced the normal ESP-IDF log sink with `esp_rom_vprintf`.

**Required behavior:** keep the normal ESP-IDF thread-safe logging path unless a proven re-entrant replacement is required.

**Hardening branch:** ROM vprintf override removed; pending next UART-log validation.

## 12. FAIL-CLOSED physical outputs — SAFETY INVARIANT

**Observed:** `missing_hardware_record` / `invalid_hardware` keeps physical outputs blocked.

This behavior is correct and is cemented as a safety invariant. Missing/rejected commissioning or hardware verification must never make physical outputs available. Cleanup and degraded-mode fixes must not weaken this gate.

**Hardening branch:** invariant retained and included in preflight contract.

## Additional Android defects confirmed by the historical review

These are outside the original twelve firmware/hardware findings but are part of the same field acceptance scope:

### A1. Cross-transport duplicate controller cards

Run 877 could represent one controller as both real `HG-*` and HTTP-fallback `setup-IP-80` identity. Physical identity must be canonicalized across mDNS/UDP/HTTP and persisted aliases must collapse without losing the owner-assigned name.

Current PR #47 head contains the post-877 `DeviceIdentity`/physical-controller dedup implementation. It remains mandatory to field-test it from a clean Android application state and with a previously polluted device store.

### A2. Normal device-list state indicators missing

Run 877 and the pre-hardening PR #47 head showed only a general HomeGuard image plus `online/offline` text in the normal card.

**Hardening branch:** compact `LAN` and `CLOUD` indicators are now implemented on the normal card without exposing ID/IP there. Technical ID/address remain in Properties. Pending APK build and phone field validation.

## Acceptance gate for the next test revision

The next candidate must demonstrate all of the following in one controlled field campaign:

- stable Wi-Fi reconnect after router/AP interruption, including bounded retry rather than a tight reconnect loop;
- recovery AP returns during STA loss and retires again only after LAN IP is restored;
- `GET /api/v1/network/status` has no Wi-Fi state-changing side effects;
- ordinary firmware update preserves previously stored Wi-Fi credentials;
- a deliberate clean-state reset erases mutable Wi-Fi/access/cloud/provisioning state;
- Android/LAN discovery still works at least as well as the field-proven Run-835 control point;
- one physical controller produces one discovery/registered card even when mDNS, UDP and HTTP all find it;
- compact LAN/local and CLOUD state indicators are visible on the normal device card;
- ESP Wi-Fi SSID scan, when explicitly tested later, does not deliberately disconnect STA;
- no GPIO ISR-service error;
- no repeated W5500 commands after W5500 init/start failure;
- no per-second INA226 error flood when INA226 is absent;
- I2C hardware is only Ready after physical ACK/proper initialization;
- missing microSD is graceful and does not create periodic runtime work;
- hardware bootstrap reports degraded state honestly;
- UART logs remain readable under concurrent tasks;
- physical outputs remain FAIL-CLOSED without valid hardware verification/commissioning;
- clean-state validation is performed only after a real reset/erase of mutable state.
