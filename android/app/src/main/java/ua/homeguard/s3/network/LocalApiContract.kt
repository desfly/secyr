package ua.homeguard.s3.network

object LocalApiContract {
    const val STATUS_PATH = "/api/v1/system/status"
    const val HEALTH_PATH = "/api/health"
    const val CHALLENGE_PATH = "/api/challenge"
    const val COMMAND_PATH = "/api/command"
    const val TELEMETRY_PATH = "/ws/system"
    fun requestId(value: Long): String = value.toString()
}
