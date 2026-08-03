# HomeGuard-S3 Build-0013

Build-0013 hardens the two runtime transitions most likely to fail during the first physical test: finishing provisioning without losing the phone response, and publishing WSS telemetry without calling the HTTPS server from the wrong FreeRTOS task.

## Implemented in this build

- successful provisioning keeps Setup AP/HTTPS alive for a 1.5-second response grace period before shutdown and restart;
- the response delay is implemented by a portable, host-tested `ProvisioningShutdownGate`;
- WSS broadcasts are queued with `httpd_queue_work` and executed in the ESP HTTP server task;
- queued frames own their JSON payload until transmission completes;
- failed WebSocket clients are removed safely;
- Wi-Fi connected/disconnect counters are atomic; IPv4 state is mutex-protected;
- every partial Wi-Fi initialization failure rolls back event handlers, Wi-Fi driver, netif and password RAM;
- Android version is `0.0.13`, `versionCode 13`;
- all unresolved HW-678 GPIO values remain `-1`.

## Existing local protocol

- `GET /api/status`
- `GET /api/health`
- `POST /api/challenge`
- `POST /api/command`
- `WSS /ws/telemetry`
- bearer-token authentication and one-time challenges for dangerous commands;
- exact DER certificate SHA-256 validation;
- mDNS/UDP discovery returning the certificate-covered `.local` hostname.

## Validation

```bash
python3 tools/validate.py
python3 tools/package.py
```

The archive is ready for the GitHub workflow **Build HomeGuard-S3 Build-0013**. No `.bin` or `.apk` is claimed until the real ESP-IDF and Android jobs finish successfully.
