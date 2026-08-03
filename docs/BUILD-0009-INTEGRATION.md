# Build-0009 integration gate

Build-0009 changes the project from optimistic stubs to explicit readiness states.

## Boot behavior

1. Validate every configured GPIO and reject duplicate or partial bus maps.
2. Derive hardware capabilities from the validated map.
3. Keep all physical outputs disabled when their pins or polarity are unverified.
4. Start secure provisioning when no encrypted credentials are present.
5. Otherwise connect Wi-Fi STA and supervise it with the task watchdog.
6. Refuse to advertise a complete local operational service until REST and WSS are implemented.

## Next code gate

Build-0013 should implement the normal HTTPS REST API and authenticated WSS telemetry on one TLS server, then add
Android integration tests against that protocol. Only after that succeeds should mDNS advertise the operational
service and the project proceed to W5500 or physical I/O drivers.
