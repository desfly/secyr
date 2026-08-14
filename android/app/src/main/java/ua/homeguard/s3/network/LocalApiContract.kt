package ua.homeguard.s3.network

object LocalApiContract {
    const val STATUS_PATH = "/api/v1/system/status"
    const val ZONES_PATH = "/api/v1/system/zones"
    const val OUTPUTS_PATH = "/api/v1/system/outputs"
    const val PARTITIONS_PATH = "/api/v1/system/partitions"
    const val HEALTH_PATH = "/api/health"
    const val CHALLENGE_PATH = "/api/challenge"
    const val COMMAND_PATH = "/api/command"
    const val TELEMETRY_PATH = "/ws/system"
    fun requestId(value: Long): String = value.toString()
}
