package ua.homeguard.s3.network

import java.net.URLEncoder

object EndpointUrlBuilder {
    fun normalizeBaseUrl(raw: String): String = raw.trim().trimEnd('/')

    fun websocketUrl(baseUrl: String): String {
        val base = normalizeBaseUrl(baseUrl)
        val websocketBase = when {
            base.startsWith("https://", ignoreCase = true) -> "wss://" + base.substring(8)
            base.startsWith("http://", ignoreCase = true) -> "ws://" + base.substring(7)
            else -> return ""
        }
        return websocketBase + RuntimeApiContract.TELEMETRY_PATH
    }

    fun cloudDeviceBase(cloudBaseUrl: String, deviceId: String): String {
        val root = normalizeBaseUrl(cloudBaseUrl)
        if (root.isBlank() || deviceId.isBlank()) return ""
        val encodedId = URLEncoder.encode(deviceId, Charsets.UTF_8.name())
        return "$root/v1/devices/$encodedId"
    }
}
