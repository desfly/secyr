package ua.homeguard.s3.network

/**
 * Legacy/cloud transport only.
 * The active local ESP runtime uses /api/v1/... and must not reference these paths.
 */
object LegacyApiContract {
    const val STATUS_PATH = "/api/status"
    const val HEALTH_PATH = "/api/health"
    const val CHALLENGE_PATH = "/api/challenge"
    const val COMMAND_PATH = "/api/command"

    fun requestId(value: Long): String = value.toString()
}
