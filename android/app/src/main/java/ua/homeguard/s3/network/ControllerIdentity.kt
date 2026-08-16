package ua.homeguard.s3.network

/**
 * Canonical identity helpers for a physical HomeGuard controller.
 * Discovery transports may disagree about scheme, port or temporary ID,
 * but one LAN host still represents one ESP controller.
 */
object ControllerIdentity {
    fun normalizeHost(host: String): String = host.trim().trim('[', ']').lowercase()

    fun hostFromBaseUrl(baseUrl: String): String {
        val authority = baseUrl.trim()
            .substringAfter("://", "")
            .substringBefore('/')
            .substringBefore('?')
            .substringBefore('#')

        if (authority.startsWith("[")) {
            return authority.substringAfter('[').substringBefore(']').lowercase()
        }
        return authority.substringBefore(':').lowercase()
    }

    fun key(deviceId: String, host: String): String {
        val normalizedHost = normalizeHost(host)
        return if (normalizedHost.isNotBlank()) "host:$normalizedHost" else "id:${deviceId.trim().lowercase()}"
    }

    fun sameController(
        leftDeviceId: String,
        leftBaseUrl: String,
        rightDeviceId: String,
        rightBaseUrl: String,
    ): Boolean {
        if (leftDeviceId.isNotBlank() && leftDeviceId == rightDeviceId) return true
        val leftHost = hostFromBaseUrl(leftBaseUrl)
        val rightHost = hostFromBaseUrl(rightBaseUrl)
        return leftHost.isNotBlank() && leftHost == rightHost
    }
}
