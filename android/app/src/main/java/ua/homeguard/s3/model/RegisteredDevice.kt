package ua.homeguard.s3.model

enum class RegisteredDeviceAccess {
    UNKNOWN,
    ONLINE,
    OFFLINE,
    ACCESS_REVOKED,
}

data class RegisteredDevice(
    val deviceId: String,
    val displayName: String,
    val lastKnownUrl: String,
    val certificateSha256: String = "",
    val access: RegisteredDeviceAccess = RegisteredDeviceAccess.UNKNOWN,
    val lastSeenAtMs: Long = 0L,
    val lastVerifiedAtMs: Long = 0L,
) {
    val accessRevoked: Boolean get() = access == RegisteredDeviceAccess.ACCESS_REVOKED

    fun renamed(name: String): RegisteredDevice {
        val normalized = name.trim()
        require(normalized.isNotEmpty()) { "Device name is required" }
        return copy(displayName = normalized.take(64))
    }
}
