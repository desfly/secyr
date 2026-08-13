package ua.homeguard.s3.repository

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.launch
import ua.homeguard.s3.model.AccessLoginRejectedException
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.RegisteredDevice
import ua.homeguard.s3.model.RegisteredDeviceAccess
import ua.homeguard.s3.storage.RegisteredDeviceStore
import ua.homeguard.s3.storage.UserCredentialStore

sealed class DeviceAddResult {
    data class Added(val device: RegisteredDevice) : DeviceAddResult()
    data object NotFound : DeviceAddResult()
    data object CredentialsRejected : DeviceAddResult()
    data object AccessRevoked : DeviceAddResult()
    data class Failed(val message: String) : DeviceAddResult()
}

class DeviceRegistryCoordinator(
    private val scope: CoroutineScope,
    private val credentials: UserCredentialStore,
    private val registry: RegisteredDeviceStore,
    private val verifier: DeviceAccessVerifier = DeviceAccessVerifier(),
    private val directIp: DirectIpEnrollmentClient = DirectIpEnrollmentClient(),
) {
    fun addIfAuthorized(
        device: DiscoveredDevice,
        displayName: String,
        certificateSha256: String = "",
        onResult: (DeviceAddResult) -> Unit = {},
    ) {
        val savedCredentials = credentials.credentials.value
            ?: return onResult(DeviceAddResult.CredentialsRejected)
        scope.launch {
            val access = verifier.verify(device, savedCredentials, certificateSha256)
            when (access) {
                RegisteredDeviceAccess.ONLINE -> onResult(DeviceAddResult.Added(save(device, displayName, certificateSha256)))
                RegisteredDeviceAccess.ACCESS_REVOKED -> onResult(DeviceAddResult.AccessRevoked)
                RegisteredDeviceAccess.CREDENTIALS_REJECTED -> onResult(DeviceAddResult.CredentialsRejected)
                else -> onResult(DeviceAddResult.Failed("Пристрій недоступний"))
            }
        }
    }

    fun addById(
        deviceId: String,
        displayName: String,
        discovered: List<DiscoveredDevice>,
        onResult: (DeviceAddResult) -> Unit = {},
    ) {
        val wanted = normalizeId(deviceId)
        val device = discovered.firstOrNull { normalizeId(it.deviceId) == wanted }
            ?: return onResult(DeviceAddResult.NotFound)
        addIfAuthorized(device, displayName, onResult = onResult)
    }

    fun addByIp(
        ip: String,
        displayName: String,
        discovered: List<DiscoveredDevice>,
        onResult: (DeviceAddResult) -> Unit = {},
    ) {
        val wanted = normalizeHost(ip)
        val discoveredDevice = discovered.firstOrNull { normalizeHost(it.host) == wanted }
        if (discoveredDevice != null) {
            addIfAuthorized(discoveredDevice, displayName, onResult = onResult)
            return
        }

        val savedCredentials = credentials.credentials.value
            ?: return onResult(DeviceAddResult.CredentialsRejected)
        scope.launch {
            try {
                val resolved = directIp.resolveAndAuthorize(ip, savedCredentials)
                onResult(DeviceAddResult.Added(save(resolved, displayName)))
            } catch (error: AccessLoginRejectedException) {
                when (error.reason) {
                    ua.homeguard.s3.model.AccessLoginFailureReason.USER_NOT_FOUND,
                    ua.homeguard.s3.model.AccessLoginFailureReason.ACCESS_REVOKED,
                    -> onResult(DeviceAddResult.AccessRevoked)
                    ua.homeguard.s3.model.AccessLoginFailureReason.BAD_CREDENTIALS,
                    ua.homeguard.s3.model.AccessLoginFailureReason.UNKNOWN,
                    -> onResult(DeviceAddResult.CredentialsRejected)
                }
            } catch (error: Throwable) {
                onResult(DeviceAddResult.Failed(error.message ?: "Пристрій недоступний"))
            }
        }
    }

    fun refresh(discovered: List<DiscoveredDevice>) {
        val savedCredentials = credentials.credentials.value ?: return
        scope.launch {
            registry.devices.value.forEach { registered ->
                val visible = discovered.firstOrNull { candidate ->
                    candidate.deviceId == registered.deviceId ||
                        (registered.deviceId.startsWith("manual:") && normalizeHost(candidate.host) == normalizeHost(registered.lastKnownUrl))
                }
                if (visible == null) {
                    registry.updateAccess(registered.deviceId, RegisteredDeviceAccess.OFFLINE)
                } else {
                    val access = verifier.verify(visible, savedCredentials, registered.certificateSha256)
                    val updated = registered.copy(
                        lastKnownUrl = visible.baseUrl,
                        access = access,
                        lastSeenAtMs = visible.seenAtMs,
                        lastVerifiedAtMs = System.currentTimeMillis(),
                    )
                    registry.upsert(updated)
                }
            }
        }
    }

    private fun save(device: DiscoveredDevice, displayName: String, certificateSha256: String = ""): RegisteredDevice {
        val saved = RegisteredDevice(
            deviceId = device.deviceId,
            displayName = displayName.trim().ifBlank { "HomeGuard" }.take(64),
            lastKnownUrl = device.baseUrl,
            certificateSha256 = certificateSha256,
            access = RegisteredDeviceAccess.ONLINE,
            lastSeenAtMs = device.seenAtMs,
            lastVerifiedAtMs = System.currentTimeMillis(),
        )
        registry.upsert(saved)
        return saved
    }

    private fun normalizeId(value: String): String = value.trim().uppercase()

    private fun normalizeHost(value: String): String = value.trim()
        .removePrefix("http://")
        .removePrefix("https://")
        .substringBefore('/')
        .substringBefore(':')
        .lowercase()
}
