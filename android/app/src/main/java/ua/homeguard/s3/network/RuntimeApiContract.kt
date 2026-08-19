package ua.homeguard.s3.network

/** Active ESP runtime contract. */
object RuntimeApiContract {
    const val ACCESS_STATE_PATH = "/api/v1/access/state"
    const val ACCESS_USERS_PATH = "/api/v1/access/users"
    const val ACCESS_LOGIN_PATH = "/api/v1/access/login"
    const val TELEMETRY_SESSION_PATH = "/api/v1/telemetry/session"
    const val SECURITY_COMMAND_PATH = "/api/v1/system/security-command"
    const val OUTPUT_COMMAND_PATH = "/api/v1/system/output-command"
    const val FACTORY_RESET_PATH = "/api/v1/system/factory-reset"
    const val NETWORK_STATUS_PATH = "/api/v1/network/status"
    const val NETWORK_SCAN_PATH = "/api/v1/network/scan"
    const val NETWORK_CONNECT_PATH = "/api/v1/network/connect"
    const val TELEMETRY_PATH = "/ws/telemetry"
}
