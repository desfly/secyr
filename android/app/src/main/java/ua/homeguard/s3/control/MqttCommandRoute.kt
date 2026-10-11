package ua.homeguard.s3.control

import kotlinx.coroutines.CancellationException
import org.json.JSONObject
import ua.homeguard.s3.model.CommandReply
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.network.mqtt.MqttCommandOutcomeUnknown

/** A null reply means this optional route is unconfigured/unavailable, not a sent command. */
class MqttCommandRoute(private val remote: suspend (String) -> JSONObject?) {
    suspend fun execute(type: CommandType): CommandReply? = when (type) {
        CommandType.ARM_HOME -> send("security.arm_home")
        CommandType.ARM_AWAY -> send("security.arm_away")
        CommandType.DISARM -> send("security.disarm")
        else -> null
    }

    suspend fun pulseLock(): CommandReply? = send("output.lock")

    private suspend fun send(command: String): CommandReply? {
        return try {
            val response = remote(command) ?: return null
            CommandReply(accepted = response.opt("ok") == true,
                code = response.optString("reason").ifBlank { if (response.opt("ok") == true) "accepted" else "rejected" })
        } catch (error: CancellationException) {
            throw error
        } catch (_: MqttCommandOutcomeUnknown) {
            CommandReply(false, code = "command_outcome_unknown")
        } catch (_: Exception) {
            // Non-null prevents a second transport from repeating a possibly sent action.
            CommandReply(false, code = "mqtt_command_failed")
        }
    }
}
