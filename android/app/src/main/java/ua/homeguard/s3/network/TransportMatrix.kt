package ua.homeguard.s3.network

import ua.homeguard.s3.model.ControlPath

enum class TransportKind {
    LOCAL_HTTP,
    LOCAL_WSS,
    BLE,
    CLOUD_HTTP,
    CLOUD_WSS,
    MQTT,
}

data class TransportStatus(
    val kind: TransportKind,
    val available: Boolean,
    val authenticated: Boolean = false,
    val lastSeenAtMs: Long = 0L,
)

object TransportMatrix {
    val commandPriority = listOf(
        TransportKind.LOCAL_HTTP,
        TransportKind.BLE,
        TransportKind.CLOUD_HTTP,
        TransportKind.MQTT,
    )

    val telemetryPriority = listOf(
        TransportKind.LOCAL_WSS,
        TransportKind.BLE,
        TransportKind.CLOUD_WSS,
        TransportKind.MQTT,
    )

    fun httpKinds(path: ControlPath): Set<TransportKind> = when (path) {
        ControlPath.LOCAL,
        ControlPath.LAST_KNOWN_LOCAL -> setOf(TransportKind.LOCAL_HTTP, TransportKind.LOCAL_WSS)
        ControlPath.CLOUD -> setOf(TransportKind.CLOUD_HTTP, TransportKind.CLOUD_WSS)
        ControlPath.OFFLINE -> emptySet()
    }

    fun chooseCommand(statuses: Collection<TransportStatus>): TransportKind? =
        choose(commandPriority, statuses)

    fun chooseTelemetry(statuses: Collection<TransportStatus>): TransportKind? =
        choose(telemetryPriority, statuses)

    private fun choose(
        priority: List<TransportKind>,
        statuses: Collection<TransportStatus>,
    ): TransportKind? {
        val byKind = statuses.associateBy { it.kind }
        return priority.firstOrNull { kind ->
            byKind[kind]?.let { it.available && it.authenticated } == true
        }
    }
}
