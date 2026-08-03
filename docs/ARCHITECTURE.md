# Architecture — Build-0013

## Runtime layers

1. `Controller` owns alarm modes, zones, pressure channels, event journal and safe outputs.
2. `HealthMonitor`, `BootSelfTest`, `NetworkFailover`, `IdempotencyCache`, `TelemetryBuilder` and `MaintenanceGuard` implement reliability and safety policies.
3. `DeviceIdentity` derives one stable `device_id` and hostname from the ESP32-S3 station MAC.
4. `DeviceDiscoveryService` advertises `_homeguard._tcp` and answers UDP discovery on 45678.
5. `CloudLink` and `CloudTransport` implement the outbound MQTTS route.

## Provisioning trust chain

1. Manufacturing creates a unique P-256 server certificate/private key, Setup AP password and pairing code.
2. The material is written to the encrypted `hg-factory` NVS namespace; only the matching QR label leaves manufacturing.
3. `ProvisioningService` starts `<device_id>-Setup` only while normal credentials are absent.
4. `ProvisioningHttpsServer` uses the factory certificate and exposes a narrow, size-bounded API.
5. Android verifies the actual server certificate SHA-256 against the physical QR and also applies normal IP-hostname verification.
6. `ProvisioningSession` verifies the pairing proof, limits attempts and accepts one validated credential payload.
7. `NvsConfigStore` commits the home Wi-Fi, optional MQTTS device token and local API token to encrypted NVS.
8. The Setup AP watchdog terminates the provisioning network and clears private RAM buffers after success, lockout or timeout.

## Android layers

1. `ProvisioningQrParser` validates the versioned QR contract.
2. `SetupNetworkConnector` obtains a user-approved Wi-Fi route to the Setup AP.
3. `PinnedProvisioningApi` performs pinned TLS authorization and provisioning.
4. `SecureTokenStore` protects the local API token with Android Keystore AES-GCM.
5. `LocalDiscoveryCoordinator` merges NSD and UDP results after provisioning.
6. `DeviceEndpointResolver` selects local, last-known-local, cloud or offline paths.
7. `RoutedDeviceApi`, `TelemetrySocket` and `OfflineCommandQueue` use that selected route.

The control API remains transport-independent: authenticated local or cloud commands converge before `Controller::execute()`.
