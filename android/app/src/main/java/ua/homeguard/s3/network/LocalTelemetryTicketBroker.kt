package ua.homeguard.s3.network

/**
 * Process-local bridge between the authenticated command session and the
 * WebSocket session manager. Telemetry handshake tickets are deliberately
 * single-use, so reconnects must mint a new ticket through the already
 * authenticated HTTP Bearer session.
 */
object LocalTelemetryTicketBroker {
    @Volatile
    private var refresher: (suspend () -> String)? = null

    fun install(value: suspend () -> String) {
        refresher = value
    }

    suspend fun refresh(): String {
        val current = refresher ?: error("local telemetry ticket refresher unavailable")
        return current()
    }
}
