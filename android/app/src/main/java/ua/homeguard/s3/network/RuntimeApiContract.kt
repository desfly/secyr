package ua.homeguard.s3.network

/** Active ESP runtime contract. */
object RuntimeApiContract {
    const val ACCESS_LOGIN_PATH = "/api/v1/access/login"
    const val TELEMETRY_SESSION_PATH = "/api/v1/telemetry/session"
    const val SECURITY_COMMAND_PATH = "/api/v1/system/security-command"
    const val OUTPUT_COMMAND_PATH = "/api/v1/system/output-command"
    const val TELEMETRY_PATH = "/ws/telemetry"
}
