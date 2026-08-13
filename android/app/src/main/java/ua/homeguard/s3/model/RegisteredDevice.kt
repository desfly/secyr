package ua.homeguard.s3.model

data class RegisteredDevice(
    val deviceId: String,
    val name: String,
    val lastKnownUrl: String = "",
    val accessState: DeviceAccessState = DeviceAccessState.ACTIVE,
    val addedAtMs: Long = System.currentTimeMillis(),
)

enum class DeviceAccessState {
    ACTIVE,
    REVOKED,
}
