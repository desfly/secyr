# HomeGuard-S3 field-test defect contract — 2026-08-16

Source baseline: Build-877 / PR #47 head before hardening.

These findings are cemented. A later revision must not regress any item that is fixed here.

## 1. Wi-Fi recovery after disconnect — P0

**Observed:** STA can drop and remain unreachable after the recovery AP has already been retired.

**Root cause:** no `WIFI_EVENT_STA_DISCONNECTED` reconnect state machine; `/api/v1/network/status` had a side effect that changed APSTA to STA; Wi-Fi scan intentionally disconnected the active STA.

**Required behavior:** automatically reconnect after a real STA loss, restore the recovery AP while disconnected, retire it only after `IP_EVENT_STA_GOT_IP`, and never disconnect an established STA just to scan.

## 2. W5500 failed-init resource leak — P0

**Observed:** repeated W5500 command timeouts continue after bootstrap already reported Ethernet failure.

**Root cause:** partially created MAC/PHY/driver/netif resources survived failure paths.

**Required behavior:** every failed init/start path performs full reverse-order rollback; no W5500 SPI activity is allowed after the driver is classified unavailable.

## 3. W5500 GPIO ISR service ordering — P0

**Observed:** `GPIO isr service is not installed` during W5500 initialization.

**Required behavior:** install the GPIO ISR service before the W5500 interrupt-backed MAC is created; `ESP_ERR_INVALID_STATE` means the shared service already exists and is acceptable.

## 4. INA226 false Ready state and log flood — P0

**Observed:** `ina226: bus voltage` error repeats once per telemetry cycle even when the device did not initialize successfully.

**Root cause:** a non-null I2C handle was treated as Ready after later configuration writes failed.

**Required behavior:** failed configuration removes the device handle, Ready requires completed initialization, and ordinary runtime read failure must not emit one ESP_ERROR per second.

## 5. I2C phantom devices — P0

**Observed:** devices can be shown Ready even though physical presence was never verified.

**Root cause:** `i2c_master_bus_add_device()` only creates a software handle; it is not a physical ACK probe.

**Required behavior:** every I2C device must pass `i2c_master_probe()` before a runtime handle is accepted.

## 6. MCP23017 partial initialization — P0/P1

**Observed risk:** MCP23017 can retain a handle after one of its configuration writes fails.

**Required behavior:** initialization is transactional; any failed direction/pull-up/safe-output write removes the handle and leaves the module unavailable. Outputs remain fail-closed.

## 7. Misleading hardware-bootstrap success — P1

**Observed:** log can say `Hardware bootstrap completed` while multiple optional modules failed.

**Required behavior:** distinguish Ready, Missing/Degraded and Fault. The boot log must explicitly say when bootstrap completed in degraded mode and report the number of unavailable/degraded optional modules.

## 8. microSD failure cleanup — P1

**Observed:** absent/bad microSD produces a low-level init failure during bootstrap.

**Required behavior:** absence is a controlled unavailable state; a failed mount must clear card state and release an SPI bus owned by the SD runtime. Telemetry must not periodically call storage refresh when no card is mounted.

## 9. Runtime must respect bootstrap state — P1

**Observed:** telemetry can continue polling hardware that bootstrap already classified Missing/Fault.

**Required behavior:** runtime sampling is gated by hardware-module state. Missing/Fault RTC, ADC, INA226, SD and other optional hardware must not be continuously polled.

## 10. Clean-state test / Factory Reset invariant — TEST RULE

**Observed:** Build-877 restored Wi-Fi and one access user because an ordinary firmware `write-flash` did not erase mutable NVS.

This is not itself a Build-877 defect. It is a mandatory test rule:

- ordinary update must preserve state when expected;
- a true Factory Reset/full clean test must erase mutable user-owned state;
- immutable device identity and installed firmware must remain intact;
- the first-Admin bootstrap must become available again after a true Factory Reset.

A firmware test may not be called `from zero` if old Wi-Fi/access state is still present.

## 11. Interleaved UART logging — P2

**Observed:** output from multiple tasks/components becomes mixed into hard-to-read lines.

**Root cause:** application replaced the normal ESP-IDF log sink with `esp_rom_vprintf`.

**Required behavior:** keep the normal ESP-IDF thread-safe logging path unless a proven re-entrant replacement is required.

## 12. FAIL-CLOSED physical outputs — SAFETY INVARIANT

**Observed:** `missing_hardware_record` / `invalid_hardware` keeps physical outputs blocked.

This behavior is correct and is cemented as a safety invariant. Missing/rejected commissioning or hardware verification must never make physical outputs available. Cleanup and degraded-mode fixes must not weaken this gate.

## Acceptance gate for the next test revision

The next firmware candidate must demonstrate all of the following in one field run:

- stable Wi-Fi reconnect after router/AP interruption;
- recovery AP returns during STA loss and retires again only after LAN IP is restored;
- network scan does not deliberately disconnect STA;
- no GPIO ISR-service error;
- no repeated W5500 commands after W5500 init/start failure;
- no per-second INA226 error flood when INA226 is absent;
- I2C hardware is only Ready after physical ACK/proper initialization;
- missing microSD is graceful and does not create periodic runtime work;
- hardware bootstrap reports degraded state honestly;
- UART logs remain readable under concurrent tasks;
- physical outputs remain FAIL-CLOSED without valid hardware verification/commissioning;
- clean-state validation is performed only after a real reset/erase of mutable state.
