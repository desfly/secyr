# Connectivity — Build-0013

## First run

```text
Physical QR
   -> Android QR validation
   -> user-approved connection to HG-S3-XXXXXX-Setup
   -> pinned HTTPS at 192.168.4.1:8443
   -> pairing authorization
   -> encrypted credential commit
   -> Setup AP/HTTPS shutdown
```

Setup AP policy: WPA2, PMF required, one client, channel 6 by default, 15-minute maximum session, no cleartext HTTP.

## Normal local route

The controller advertises one stable device identity across Ethernet and Wi-Fi:

```text
mDNS _homeguard._tcp -> UDP/45678 fallback -> last known local URL
```

## Remote route

The controller establishes an outbound MQTTS session after cloud credentials are provisioned. The app uses its account/cloud HTTPS route and selects the same stable `device_id`. The ESP32 does not require a public IP, port forwarding or an inbound internet listener.

## Android route priority

```text
local discovery -> last-known local -> cloud -> offline queue
```
