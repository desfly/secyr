# HomeGuard-S3 — audit journal — 2026-08-23 continuation 02

## User-reproduced defect

Hardware test on ESP32-S3 Build-1961 (`b1613854cbe4ae1af734068816646b0c9513063e`) confirmed a first-boot handover defect:

- after Admin creation the HomeGuard setup SoftAP was disabled before the station interface had a usable IPv4 address;
- the user could therefore lose the setup AP before the configured home Wi-Fi path was proven reachable;
- UART showed the bad ordering on the reproduced boot: setup AP was closed at about 1280 ms while STA received IPv4 only around 3780 ms.

## Firmware correction

Commit `5a8009102bb70d681b4767942e26c306d061df30` (`Keep setup AP until STA has IPv4`) changes the handover invariant:

1. Admin/bootstrap must be locked.
2. `WIFI_STA_DEF` must exist.
3. `esp_netif_get_ip_info()` must report a non-zero IPv4 address.
4. Only then may the guard call `esp_wifi_set_mode(WIFI_MODE_STA)` and remove the HomeGuard setup SoftAP.
5. If STA has no IPv4, the setup AP remains active as the recovery path.

Expected UART ordering after the correction:

```text
Admin ready but STA has no IPv4; keeping setup AP active until network handover succeeds
wifi:connected with <configured SSID>
sta ip: <IPv4>
STA IPv4 ready; closing setup AP
Open setup AP disabled; Wi-Fi is now STA-only
```

## CI regression gate

Commit `8e7adc6aa9b2bf51c7cdc863cbe7ce23fbd483a6` (`Gate setup AP shutdown on STA IPv4`) adds a preflight source contract that fails CI if:

- the STA IPv4 readiness helper is removed;
- setup AP shutdown is allowed merely because bootstrap/Admin is locked;
- the shutdown call is ordered before the STA IPv4 readiness gate.

## Build trigger

This journal commit intentionally creates a fresh `push` on `main` so `.github/workflows/homeguard-build.yml` runs against the corrected firmware plus the new regression gate.

Target for the next hardware test: firmware built from this commit or a direct descendant containing both `5a800910` and `8e7adc6`.

## Validation status

- Code correction: PRESENT.
- CI regression gate: PRESENT.
- Hardware validation of the corrected handover: PENDING next green firmware artifact.
