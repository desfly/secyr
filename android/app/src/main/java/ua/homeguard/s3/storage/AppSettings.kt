package ua.homeguard.s3.storage

data class AppSettings(
    val deviceId: String = "",
    val apiToken: String = "",
    val autoReconnect: Boolean = true,
    val remoteAccessEnabled: Boolean = false,
    val cloudBaseUrl: String = "",
    val lastKnownLocalUrl: String = "",
    val localCertificateSha256: String = "",
    val criticalNotificationsEnabled: Boolean = true,
    val statusNotificationsEnabled: Boolean = true,
    val zoneNotificationsEnabled: Boolean = true,
)
