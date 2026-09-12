package ua.homeguard.s3.control

import ua.homeguard.s3.model.CommandReply
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.network.ble.BleRuntimeSession

/**
 * Executes commands through independent transports without coupling their runtimes.
 *
 * HTTP remains the primary command path. If it is unavailable (or reports offline),
 * authenticated BLE is tried for commands that the BLE runtime currently supports.
 * Cloud HTTP and local HTTP remain owned by CommandController; MQTT is intentionally
 * not tunneled through either of them and will be added as its own route.
 */
class CommandTransportRouter(
    private val http: CommandController,
    private val ble: BleRuntimeSession,
) {
    suspend fun execute(
        type: CommandType,
        actor: String = "",
        credential: String = "",
    ): CommandReply {
        val httpReply = runCatching { http.execute(type, actor, credential) }.getOrNull()
        if (httpReply != null && httpReply.code != "offline") return httpReply

        if (!supportsBle(type)) return httpReply ?: CommandReply(false, code = "offline")
        val bleReply = runCatching { ble.execute(type) }.getOrElse {
            return httpReply ?: CommandReply(false, code = "offline")
        }
        return CommandReply(
            accepted = bleReply.optBoolean("ok", false),
            duplicate = bleReply.optBoolean("duplicate", false),
            code = bleReply.optString("code").ifBlank {
                bleReply.optString("reason").ifBlank { if (bleReply.optBoolean("ok", false)) "ok" else "rejected" }
            },
        )
    }

    private fun supportsBle(type: CommandType): Boolean = when (type) {
        CommandType.ARM_HOME,
        CommandType.ARM_AWAY,
        CommandType.DISARM,
        CommandType.OPEN_VALVES,
        CommandType.CLOSE_VALVES,
        -> true
        else -> false
    }
}
