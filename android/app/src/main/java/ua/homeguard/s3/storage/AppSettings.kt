package ua.homeguard.s3.storage

data class AppSettings(
    val deviceId: String = "",
    val apiToken: String = "",
    val telemetryToken: String = "",
    val autoReconnect: Boolean = true,
    val remoteAccessEnabled: Boolean = false,
    val cloudBaseUrl: String = "",
    val cloudAccountEmail: String = "",
    val lastKnownLocalUrl: String = "",
    val localCertificateSha256: String = "",
    val mqttBrokerUri: String = "",
    val mqttUsername: String = "",
    val criticalNotificationsEnabled: Boolean = true,
    val statusNotificationsEnabled: Boolean = true,
    val zoneNotificationsEnabled: Boolean = true,
)
