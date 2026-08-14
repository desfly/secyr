package ua.homeguard.s3.storage

data class DeviceSelectionState(
    val deviceId: String,
    val lastKnownLocalUrl: String,
    val localCertificateSha256: String,
)

object DeviceSelectionPolicy {
    fun select(
        currentDeviceId: String,
        currentLocalUrl: String,
        currentCertificateSha256: String,
        nextDeviceId: String,
        confirmedLocalUrl: String? = null,
    ): DeviceSelectionState {
        val switchingDevice = currentDeviceId != nextDeviceId
        val confirmed = confirmedLocalUrl
            ?.trim()
            ?.trimEnd('/')
            ?.takeIf { it.isNotBlank() }

        val localUrl = confirmed ?: if (switchingDevice) "" else currentLocalUrl.trim().trimEnd('/')
        val certificate = if (switchingDevice || localUrl.isBlank()) "" else currentCertificateSha256

        return DeviceSelectionState(
            deviceId = nextDeviceId,
            lastKnownLocalUrl = localUrl,
            localCertificateSha256 = certificate,
        )
    }
}
