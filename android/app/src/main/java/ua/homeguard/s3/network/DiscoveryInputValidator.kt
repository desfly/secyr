package ua.homeguard.s3.network

import java.net.URI

object DiscoveryInputValidator {
    private val deviceIdPattern = Regex("[A-Za-z0-9._-]{3,64}")

    fun normalizeManualAddress(rawAddress: String): String? {
        val clean = rawAddress.trim().trimEnd('/')
        if (clean.isBlank() || clean.any(Char::isWhitespace)) return null
        val candidate = if (clean.startsWith("http://", true) || clean.startsWith("https://", true)) {
            clean
        } else {
            "http://$clean"
        }

        return runCatching {
            val uri = URI(candidate)
            val host = uri.host?.trim().orEmpty()
            val port = uri.port
            require(host.isNotBlank())
            require(port == -1 || port in 1..65535)
            require(uri.rawUserInfo == null && uri.rawQuery == null && uri.rawFragment == null)
            require(uri.path.isNullOrBlank() || uri.path == "/")
            val scheme = uri.scheme.lowercase()
            require(scheme == "http" || scheme == "https")
            "$scheme://$host${if (port == -1) "" else ":$port"}"
        }.getOrNull()
    }

    fun normalizeDeviceId(rawDeviceId: String): String? {
        val deviceId = rawDeviceId.trim()
        return deviceId.takeIf(deviceIdPattern::matches)
    }
}
