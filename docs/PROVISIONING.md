# Secure first-run provisioning — Build-0013

## User flow

1. The unprovisioned controller loads a per-device certificate, private key, eight-digit pairing code and WPA2 Setup AP password from the encrypted `hg-factory` NVS namespace.
2. It creates `<device_id>-Setup`, accepts one phone, and starts HTTPS on `192.168.4.1:8443`.
3. The user scans the physical QR label. The QR contains the device ID, Setup SSID/password, setup URL, certificate SHA-256 fingerprint and one-time pairing code. It never contains the home Wi-Fi password, local API token or cloud device token.
4. Android requests the required Wi-Fi/camera permissions, joins the Setup AP and accepts only the certificate whose SHA-256 fingerprint matches the QR. The generated certificate also contains `subjectAltName=IP:192.168.4.1`.
5. `/v1/provisioning/authorize` validates the code and certificate fingerprint in constant time. The code expires after 10 minutes; five failed attempts lock the session.
6. `/v1/provisioning/apply` accepts at most 2048 bytes, validates all fields and writes Wi-Fi/cloud/local API secrets to encrypted NVS. The Setup AP is stopped after a successful commit.
7. Android stores only its local API token in an AES-256-GCM key protected by Android Keystore. The device cloud token is not persisted by the application.

## Factory identity

Run outside source control:

```bash
python3 tools/make_factory_bundle.py --device-id HG-S3-7A31BC --out factory/HG-S3-7A31BC
```

The directory contains a private key, certificate, NVS-generator CSV and QR URI. Treat the entire directory as a secret manufacturing artifact. Do not add it to Git or a release ZIP.

Generate the `hg-factory` NVS image with the ESP-IDF `nvs_partition_gen.py` tool, then flash it into the encrypted NVS partition as part of manufacturing. The normal firmware does not provide a remote API to write or replace factory identity.

## Reset rule

Deleting provisioned Wi-Fi/cloud credentials requires a locally verified physical-presence signal. Network commands alone cannot invoke `erase_provisioning()` or factory reset.

## Current hardware dependency

The QR label must be created after the final station MAC/device ID is known. Before energizing outputs, define the actual reset button/maintenance input and its required hold duration in the confirmed HW-678 pin map.
