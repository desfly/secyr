package ua.homeguard.s3.network.ble

import android.content.Context
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.filterNotNull
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.withTimeout
import org.json.JSONObject
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.SystemSnapshot

/**
 * Owns the authenticated runtime BLE link to one HomeGuard controller.
 *
 * The PIN is used only while creating the BLE session and is never retained by
 * this class. Once HELLO_SESSION succeeds, the ESP binds authorization to the
 * current BLE connection epoch and normal telemetry/commands use that session.
 */
class BleRuntimeSession(context: Context) {
    private val appContext = context.applicationContext
    private val scanner = BleProvisioningScanner(appContext)
    private val client = BleHomeGuardClient(appContext)

    fun state(): StateFlow<BleHomeGuardClient.State> = client.state()
    fun snapshots(): StateFlow<SystemSnapshot> = client.snapshots()
    fun commandReplies(): StateFlow<JSONObject?> = client.commandReplies()
    fun isReady(): Boolean = client.state().value == BleHomeGuardClient.State.READY

    suspend fun connect(deviceId: String, timeoutMs: Long = 15_000L) {
        require(deviceId.isNotBlank()) { "BLE device id is empty" }
        if (client.state().value == BleHomeGuardClient.State.READY ||
            client.state().value == BleHomeGuardClient.State.CONNECTED) return

        val device = scanner.find(deviceId, timeoutMs.coerceAtMost(12_000L))
        client.connect(device)
        withTimeout(timeoutMs) {
            client.state().filter { state ->
                when (state) {
                    BleHomeGuardClient.State.CONNECTED,
                    BleHomeGuardClient.State.READY -> true
                    BleHomeGuardClient.State.ERROR,
                    BleHomeGuardClient.State.OFFLINE -> throw IllegalStateException("BLE connection failed: $state")
                    else -> false
                }
            }.first()
        }
    }

    suspend fun authenticate(actor: String, pin: String, timeoutMs: Long = 8_000L): JSONObject {
        require(actor.isNotBlank()) { "BLE actor is empty" }
        require(pin.length in 4..12 && pin.all(Char::isDigit)) { "Invalid BLE PIN" }
        require(client.authenticate(actor, pin)) { "BLE authentication could not start" }

        val reply = withTimeout(timeoutMs) {
            client.sessionReplies().filterNotNull().first()
        }
        require(reply.optBoolean("ok", false)) {
            "BLE authentication rejected: ${reply.optString("reason", "unauthorized")}"
        }
        withTimeout(timeoutMs) {
            client.state().filter { it == BleHomeGuardClient.State.READY }.first()
        }
        return reply
    }

    suspend fun connectAndAuthenticate(
        deviceId: String,
        actor: String,
        pin: String,
        connectTimeoutMs: Long = 15_000L,
        authTimeoutMs: Long = 8_000L,
    ): JSONObject {
        connect(deviceId, connectTimeoutMs)
        if (isReady()) {
            return JSONObject()
                .put("ok", true)
                .put("state", "authenticated")
                .put("actor", actor.trim())
                .put("transport", "ble")
        }
        return authenticate(actor, pin, authTimeoutMs)
    }

    suspend fun execute(type: CommandType, timeoutMs: Long = 8_000L): JSONObject {
        require(isReady()) { "BLE runtime is not authenticated" }
        val queued = when (type) {
            CommandType.ARM_HOME -> client.armHome()
            CommandType.ARM_AWAY -> client.armAway()
            CommandType.DISARM -> client.disarm()
            else -> false
        }
        require(queued) { "Command $type is not available over BLE runtime yet" }
        return awaitCommandReply(timeoutMs)
    }

    suspend fun panic(timeoutMs: Long = 8_000L): JSONObject {
        require(isReady()) { "BLE runtime is not authenticated" }
        require(client.panic()) { "BLE panic command could not start" }
        return awaitCommandReply(timeoutMs)
    }

    suspend fun sendCommand(
        command: String,
        arguments: JSONObject = JSONObject(),
        timeoutMs: Long = 8_000L,
    ): JSONObject {
        require(isReady()) { "BLE runtime is not authenticated" }
        require(command.isNotBlank()) { "BLE command is empty" }
        require(client.sendCommand(command, arguments)) { "BLE command could not start" }
        return awaitCommandReply(timeoutMs)
    }

    suspend fun controlOutput(
        outputId: Int,
        active: Boolean,
        alarmActive: Boolean = false,
        timeoutMs: Long = 8_000L,
    ): JSONObject {
        require(isReady()) { "BLE runtime is not authenticated" }
        require(outputId in 1..65535) { "Invalid output id" }
        require(client.controlOutput(outputId, active, alarmActive)) { "BLE output command could not start" }
        return awaitCommandReply(timeoutMs)
    }

    fun disconnect() = client.disconnect()

    private suspend fun awaitCommandReply(timeoutMs: Long): JSONObject = withTimeout(timeoutMs) {
        client.commandReplies().filterNotNull().first()
    }
}
