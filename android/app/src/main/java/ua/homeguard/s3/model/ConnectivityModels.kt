package ua.homeguard.s3.model

enum class ControlPath { LOCAL, LAST_KNOWN_LOCAL, CLOUD, OFFLINE }

data class DiscoveredDevice(
    val deviceId: String,
    val serviceName: String,
    val host: String,
    val port: Int,
    val secure: Boolean,
    val apiVersion: Int = 1,
    val transport: Transport = Transport.NONE,
    val pairingRequired: Boolean = false,
    val source: DiscoverySource,
    val seenAtMs: Long = System.currentTimeMillis()
) {
    val baseUrl: String
        get() {
            val formattedHost = if (host.contains(':') && !host.startsWith("[")) "[$host]" else host
            return "${if (secure) "https" else "http"}://$formattedHost:$port"
        }
}

enum class DiscoverySource { MDNS, UDP }

data class DeviceEndpoint(
    val deviceId: String,
    val apiBaseUrl: String,
    val websocketUrl: String,
    val path: ControlPath,
    val certificateSha256: String = ""
)
