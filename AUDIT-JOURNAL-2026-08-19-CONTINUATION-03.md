# HomeGuard-S3 Audit Journal — Continuation 03 — 2026-08-19

Repository: `desfly/secyr`
Branch: `main`
Primary journal: `AUDIT-JOURNAL-2026-08-19.md`
Previous continuation: `AUDIT-JOURNAL-2026-08-19-CONTINUATION-02.md`

## Fixes completed in this continuation

### Telemetry WebSocket handshake tickets

The authenticated `/ws/telemetry` correctly uses the HTTP `Authorization: Bearer` header and `BearerTokenVerifier`, not a token in the URL. `BearerTokenVerifier` hashes tokens with SHA-256 and compares digests through `constant_time_equal`.

However, telemetry session tokens issued after login had no expiry and were reusable until displaced from a four-entry ring or reboot. They are now short-lived handshake tickets:

- 60-second TTL using `esp_timer_get_time()`;
- ticket issue timestamps are tracked per slot;
- expired tickets are cleared;
- a ticket is consumed on the first successful WebSocket upgrade;
- persistent local API bearer token behavior is unchanged.

Commits:
- `730a3d1a` — header/state for telemetry token expiry and consumption
- `e1581b10` — enforce TTL and single-use behavior
- `aed3f7d0` — declare the newly required `esp_timer` component dependency

### ESP-IDF component dependency audit

The existing dependency audit only inspected the `main` component, so separate ESP-IDF components could add headers/APIs without declaring their component dependencies. It now traverses `firmware/esp-idf/components/*`, validates source references, and checks include-to-component dependencies including `esp_timer` and `esp_https_server`.

Commit:
- `e1ca6617` — audit dependencies across ESP-IDF components

### Partial HTTP server startup rollback

`start_http_server()` previously started ESP-IDF httpd and then registered routes sequentially. If any later route registration failed, the function returned an error but the already-started partial HTTP server remained alive. A particularly dangerous case occurred after `SystemHttp` registration because it had already subscribed to the system event bus and retained the server handle.

The startup path now:

1. starts httpd;
2. registers each route group explicitly;
3. on any registration failure, detaches `SystemHttp` transport, stops the partial httpd instance, clears `g_http_server`, and returns the original failure;
4. includes the final Build route in the same rollback contract.

Commits:
- `a030eb61` / `2e5bb079` — add safe `SystemHttp::detach_transport()`
- `de6ad489` — rollback partially registered HTTP server

### Source-audit guards

Static audit now protects:

- telemetry handshake ticket TTL;
- single-use session tickets;
- presence of issue timestamps;
- partial HTTP registration rollback (`detach_transport`, `httpd_stop`, null handle);
- final Build route participating in rollback;
- stale `hg_commissioning_http.hpp` remaining deleted;
- telemetry login raw-body and issued-token scrubbing.

Commit:
- `aa89822e` — guard HTTP rollback and telemetry ticket lifetime

### Stale commissioning API removal

`hg_commissioning_http.hpp` declared an HTTP class that had no `.cpp`, was not compiled by CMake, and was never instantiated by `app_main`. The actual commissioning state path is `CommissioningNvsStore` + `ServiceHttp`. The stale header was removed to prevent false API assumptions.

Commit:
- `b1b43986` — remove stale unimplemented commissioning HTTP API

### Secret lifetime cleanup

Telemetry login now scrubs the raw credential-bearing POST body and scrubs the issued session token after the JSON response is sent.

Service invalidate now scrubs the raw POST body as soon as actor/credential are parsed, in addition to scrubbing the parsed credential.

Commits:
- `530dbfe7` — scrub telemetry session secrets after use
- `d113224e` — scrub service invalidate request body

## Security finding still open — backend session boundary

The Web login endpoint authenticates credentials and returns actor/role/capabilities, but it does not establish a backend HTTP session. Several read-only monitoring endpoints are therefore currently callable directly on the LAN without passing the Web login state, including system monitoring/status-style endpoints.

This must not be "fixed" only in JavaScript because UI hiding is not authorization. A proper backend session/token/cookie boundary should be designed and applied consistently to protected read-only REST and WebSocket surfaces while retaining explicitly public provisioning/discovery endpoints.

Status: **open, high priority**. Do not claim Web login provides backend isolation yet.

## Additional active work

- Continue scrubbing raw secret-bearing POST bodies in Access, Network, Cloud and Output handlers.
- Continue POST/persistence atomicity audit.
- Add regression guards for every secret-lifetime fix.
- Design protected-vs-public HTTP endpoint matrix before adding backend session enforcement.
- Verify actual GitHub Actions / ESP-IDF 5.4.4 build. Connector still has not supplied a successful Actions run, so CI remains **unconfirmed**.

## Resume point

Continue from: **raw-body secret cleanup -> endpoint protection matrix -> backend session layer -> remaining persistence/lifecycle paths -> real CI confirmation**.
