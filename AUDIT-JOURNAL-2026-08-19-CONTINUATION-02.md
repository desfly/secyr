# HomeGuard-S3 Audit Journal — Continuation 02 — 2026-08-19

Repository: `desfly/secyr`
Branch: `main`

This is an additive continuation log. The primary handoff remains `AUDIT-JOURNAL-2026-08-19.md`.

## Findings fixed in this continuation

### Android physical-device deduplication

Old discovery grouping treated host/IP as the primary identity, so one ESP could become two logical devices after DHCP address changes. A simple deviceId-only fix was also unsafe because HTTP setup discovery can carry a temporary `setup-*` identity.

Fixed by grouping discovery candidates as one connected physical controller when they share either a stable non-setup deviceId or the same normalized host. This also collapses the bridge case `stable-ID@old-IP -> stable-ID@new-IP -> setup-ID@new-IP` into one UI device. A regression contract now protects the rule.

### Partial Factory Reset recovery

Both triple-RST and Web factory reset erase multiple NVS namespaces sequentially. A later erase can fail after an earlier destructive erase has committed.

Triple-RST previously could return from `app_main()` after such a partial reset and strand normal boot. Web reset could return HTTP 500 while stale users/config remained live in RAM although NVS was already partly erased.

Both paths now force a recovery reboot after partial destructive erase. Web UI distinguishes partial reset/recovery from a rejected reset. Static audit guards protect both paths.

### Wi-Fi configuration atomicity

`hg_network_http.cpp` used a one-shot `httpd_req_recv()`, even though HTTP/TCP request bodies can arrive in short reads. It also changed the live STA config before NVS persistence was known-good.

The handler now reads exactly the declared body length. On NVS save failure it restores the previous live STA config, keeping runtime and next-boot state consistent. Static guards protect this behavior.

### Cloud/MQTT configuration atomicity

`hg_cloud_http.cpp` had the same one-shot request-body bug. It also persisted a new MQTT config before attempting runtime start; on start failure the API reported failure but the failed config remained stored for reboot.

The handler now reads the complete body, snapshots the previous config, and on MQTT start failure restores both persistent and live previous Cloud state. Rollback failure is reported separately. Static guards protect this path.

### Physical output command parsing

`hg_output_http.cpp` used a one-shot body read and numeric/boolean parsing that rejected valid JSON containing whitespace after `:`. Admin credentials also remained in memory for the rest of the handler.

The handler now reads the complete body, accepts JSON whitespace, parses escaped string values, and scrubs the credential immediately after authorization.

### Telemetry and service credential handling

Telemetry session and service invalidate endpoints used the older string parser that did not understand escaped JSON strings, and credentials remained in strings after authentication/authorization.

Both paths now use escape-aware parsing and scrub credential buffers immediately after the auth decision.

### Web/CI coverage

- `web/factory-reset.js` is now included in the explicit Node syntax gate.
- Partial Factory Reset recovery UX is protected by source audit.
- `esp_rom_delay_us()` now requires the proper `esp_rom_sys.h` include in source audit.

## Commits in this continuation

- `b99482ac` — Guard `esp_rom_delay_us` include in source audit
- `27152116` — Cover factory reset module in Web UI syntax gate
- `ec72929b` — Reboot after partial triple-RST factory reset
- `dbd8106f` — Reboot after partial Web factory reset
- `197d75f0` — Guard partial factory reset recovery paths
- `645ad4fd` — Deduplicate Android discovery by stable identity or host
- `44603dc2` — Guard Android physical-device deduplication
- `5a125f56` — Report partial factory reset recovery in Web UI
- `bedc3db2` — Make Wi-Fi configuration input and persistence atomic
- `885e5ae2` — Guard atomic Wi-Fi configuration path
- `482f8448` — Make Cloud configuration input and runtime persistence atomic
- `1277e8c7` — Guard atomic Cloud configuration path
- `2c5462c5` — Harden physical output command request parsing
- `435e4e96` — Harden telemetry session credential parsing and scrubbing
- `5a87d19a` — Harden service credential parsing and scrubbing

## Open findings / active audit targets

1. Access capability action mismatch: login capabilities currently checks `system.network.configure`, while the real Wi-Fi endpoint authorizes `network.configure`. Built-in Admin semantics currently mask this, but the contract is inconsistent and should be aligned.
2. Old JSON string parsers remain in some auth endpoints, notably Access/System, and do not decode escaped JSON strings correctly.
3. `/api/v1/lan/scan` performs a synchronous ARP-cache stimulus across up to 254 hosts and waits 250 ms. It currently lacks a scan cooldown/rate limit; repeated local requests can consume the HTTP task.
4. Continue auditing all POST handlers for one-shot `httpd_req_recv`, credential lifetime, runtime-before-persist mutation, and partial destructive operations.
5. CI status remains unconfirmed through the available connector. Do not call the tree green until actual Actions job/run output is visible.

## Resume point

Continue from: **LAN scan throttling, Access/System JSON/auth contract alignment, then remaining POST/persistence paths.**
