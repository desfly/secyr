package ua.homeguard.s3.repository

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.RegisteredDevice
import ua.homeguard.s3.model.RegisteredDeviceAccess
import ua.homeguard.s3.storage.RegisteredDeviceStore
import ua.homeguard.s3.storage.UserCredentialStore

class DeviceRegistryCoordinator(
    private val scope: CoroutineScope,
    private val credentials: UserCredentialStore,
    private val registry: RegisteredDeviceStore,
    private val verifier: DeviceAccessVerifier = DeviceAccessVerifier(),
) {
    fun addIfAuthorized(
        device: DiscoveredDevice,
        displayName: String,
        certificateSha256: String = "",
        onResult: (Boolean) -> Unit = {},
    ) {
        val savedCredentials = credentials.credentials.value ?: return onResult(false)
        scope.launch {
            val access = verifier.verify(device, savedCredentials, certificateSha256)
            if (access == RegisteredDeviceAccess.ONLINE) {
                registry.upsert(
                    RegisteredDevice(
                        deviceId = device.deviceId,
                        displayName = displayName.trim().ifBlank { "HomeGuard" }.take(64),
                        lastKnownUrl = device.baseUrl,
                        certificateSha256 = certificateSha256,
                        access = RegisteredDeviceAccess.ONLINE,
                        lastSeenAtMs = device.seenAtMs,
                        lastVerifiedAtMs = System.currentTimeMillis(),
                    )
                )
                onResult(true)
            } else {
                onResult(false)
            }
        }
    }

    fun addById(
        deviceId: String,
        displayName: String,
        discovered: List<DiscoveredDevice>,
        onResult: (Boolean) -> Unit = {},
    ) {
        val wanted = normalizeId(deviceId)
        val device = discovered.firstOrNull { normalizeId(it.deviceId) == wanted }
            ?: return onResult(false)
        addIfAuthorized(device, displayName, onResult = onResult)
    }

    fun addByIp(
        ip: String,
        displayName: String,
        discovered: List<DiscoveredDevice>,
        onResult: (Boolean) -> Unit = {},
    ) {
        val wanted = normalizeHost(ip)
        val device = discovered.firstOrNull { normalizeHost(it.host) == wanted }
            ?: return onResult(false)
        addIfAuthorized(device, displayName, onResult = onResult)
    }

    fun refresh(discovered: List<DiscoveredDevice>) {
        val savedCredentials = credentials.credentials.value ?: return
        scope.launch {
            registry.devices.value.forEach { registered ->
                val visible = discovered.firstOrNull { it.deviceId == registered.deviceId }
                if (visible == null) {
                    // Network loss is never treated as revoked access.
                    registry.updateAccess(registered.deviceId, RegisteredDeviceAccess.OFFLINE)
                } else {
                    val access = verifier.verify(visible, savedCredentials, registered.certificateSha256)
                    registry.upsert(
                        registered.copy(
                            lastKnownUrl = visible.baseUrl,
                            access = access,
                            lastSeenAtMs = visible.seenAtMs,
                            lastVerifiedAtMs = System.currentTimeMillis(),
                        )
                    )
                }
            }
        }
    }

    private fun normalizeId(value: String): String = value.trim().uppercase()

    private fun normalizeHost(value: String): String = value.trim()
        .removePrefix("http://")
        .removePrefix("https://")
        .substringBefore('/')
        .substringBefore(':')
        .lowercase()
}
