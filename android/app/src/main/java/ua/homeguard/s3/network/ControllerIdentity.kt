package ua.homeguard.s3.network

/**
 * Canonical identity helpers for a physical HomeGuard controller.
 * Discovery transports may disagree about scheme, port, letter case or temporary ID,
 * but one LAN host/stable ID still represents one ESP controller.
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
        val leftId = leftDeviceId.trim()
        val rightId = rightDeviceId.trim()
        if (leftId.isNotBlank() && rightId.isNotBlank() && leftId.equals(rightId, ignoreCase = true)) return true

        val leftHost = hostFromBaseUrl(leftBaseUrl)
        val rightHost = hostFromBaseUrl(rightBaseUrl)
        return leftHost.isNotBlank() && rightHost.isNotBlank() && leftHost == rightHost
    }
}
