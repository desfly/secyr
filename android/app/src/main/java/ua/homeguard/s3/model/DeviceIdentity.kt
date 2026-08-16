package ua.homeguard.s3.model

import java.net.URI

/** Stable identity helpers used by discovery, persistence and UI matching. */
object DeviceIdentity {
    private val macLike = Regex("(?i)([0-9a-f]{12})$")

    fun canonicalId(value: String): String {
        val clean = value.trim().lowercase()
        if (clean.isBlank()) return ""
        val compact = clean.replace(Regex("[^0-9a-z]"), "")
        val suffix = macLike.find(compact)?.groupValues?.getOrNull(1)
        return if (!suffix.isNullOrBlank()) "hw:$suffix" else "id:$clean"
    }

    fun endpointHost(value: String): String {
        val clean = value.trim().trimEnd('/')
        if (clean.isBlank()) return ""
        return runCatching {
            val withScheme = if (clean.contains("://")) clean else "http://$clean"
            URI(withScheme).host?.lowercase().orEmpty()
        }.getOrDefault("")
    }

    fun isForbiddenFriendlyName(value: String, deviceId: String = "", baseUrl: String = ""): Boolean {
        val clean = value.trim()
        if (clean.isBlank()) return true
        if (clean.equals("HomeGuard", ignoreCase = true)) return true
        if (clean.equals("HomeGuard-S3", ignoreCase = true)) return true
        if (deviceId.isNotBlank() && clean.equals(deviceId.trim(), ignoreCase = true)) return true

        val endpoint = baseUrl.trim().trimEnd('/')
        if (endpoint.isNotBlank() && clean.equals(endpoint, ignoreCase = true)) return true
        val host = endpointHost(endpoint)
        if (host.isNotBlank() && clean.equals(host, ignoreCase = true)) return true
        return false
    }

    fun samePhysicalDevice(
        firstId: String,
        firstUrl: String,
        secondId: String,
        secondUrl: String,
    ): Boolean {
        val aId = canonicalId(firstId)
        val bId = canonicalId(secondId)
        if (aId.isNotBlank() && bId.isNotBlank() && aId == bId) return true

        val aHost = endpointHost(firstUrl)
        val bHost = endpointHost(secondUrl)
        return aHost.isNotBlank() && bHost.isNotBlank() && aHost == bHost
    }
}
