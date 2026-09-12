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

data class TransportSelection(
    val command: TransportKind?,
    val telemetry: TransportKind?,
) {
    val localActive: Boolean
        get() = command == TransportKind.LOCAL_HTTP || telemetry == TransportKind.LOCAL_WSS

    val bleActive: Boolean
        get() = command == TransportKind.BLE || telemetry == TransportKind.BLE

    val cloudActive: Boolean
        get() = command == TransportKind.CLOUD_HTTP ||
            command == TransportKind.MQTT ||
            telemetry == TransportKind.CLOUD_WSS ||
            telemetry == TransportKind.MQTT

    val primary: TransportKind?
        get() = command ?: telemetry
}

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

    fun chooseActive(statuses: Collection<TransportStatus>): TransportSelection =
        TransportSelection(
            command = chooseCommand(statuses),
            telemetry = chooseTelemetry(statuses),
        )

    fun isUsable(status: TransportStatus): Boolean =
        status.available && status.authenticated

    private fun choose(
        priority: List<TransportKind>,
        statuses: Collection<TransportStatus>,
    ): TransportKind? {
        val byKind = statuses.associateBy { it.kind }
        return priority.firstOrNull { kind ->
            byKind[kind]?.let(::isUsable) == true
        }
    }
}
