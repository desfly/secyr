# Security properties — Build-0013

- No cleartext HTTP in Android; local control and provisioning use HTTPS/WSS.
- The setup certificate is authenticated by an exact SHA-256 fingerprint carried in the physical QR label; the certificate also has an IP SAN.
- Pairing proof uses SHA-256 and constant-time digest comparison.
- Pairing has a bounded lifetime, attempt limit and lockout state.
- Setup AP uses WPA2, one randomly generated per-device password and one allowed client.
- Request bodies are bounded to 2048 bytes and are zeroed after parsing.
- Wi-Fi, cloud and local API secrets are stored in ESP encrypted NVS.
- Android API tokens are encrypted with AES-GCM under an Android Keystore key.
- Factory private keys and QR secrets are generated per device and excluded from repository/release archives.
- Factory reset requires physical presence; remote reset is not implemented.

Build-0013 does not yet enable ESP Secure Boot or flash encryption for the full application image. Those should be enabled and fused only after recovery, signing and production-key procedures are tested on spare hardware.
