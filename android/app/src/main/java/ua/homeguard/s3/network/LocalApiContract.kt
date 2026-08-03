package ua.homeguard.s3.network

object LocalApiContract {
    const val STATUS_PATH = "/api/status"
    const val HEALTH_PATH = "/api/health"
    const val CHALLENGE_PATH = "/api/challenge"
    const val COMMAND_PATH = "/api/command"
    const val TELEMETRY_PATH = "/ws/telemetry"
    fun requestId(value: Long): String = value.toString()
}
