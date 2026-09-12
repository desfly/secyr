package ua.homeguard.s3.network.ble

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.filterNotNull
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.withTimeout
import org.json.JSONObject
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.SystemSnapshot

data class BleSessionAccess(
    val actor: String = "",
    val name: String = "",
    val role: String = "",
    val monitor: Boolean = false,
    val armHome: Boolean = false,
    val armAway: Boolean = false,
    val disarm: Boolean = false,
    val panic: Boolean = false,
    val valves: Boolean = false,
    val networkConfigure: Boolean = false,
    val accessManage: Boolean = false,
    val serviceInvalidate: Boolean = false,
) {
    val authenticated: Boolean get() = actor.isNotBlank()

    fun allows(command: String): Boolean = when (command) {
        "security.arm_home" -> armHome
        "security.arm_away" -> armAway
        "security.disarm" -> disarm
        "security.panic" -> panic
        "valve.open", "valve.close" -> valves
        "network.configure" -> networkConfigure
        "access.manage" -> accessManage
        "system.service.invalidate" -> serviceInvalidate
        else -> true // ESP remains authoritative for commands not represented in HELLO capabilities.
    }

    companion object {
        fun fromHello(reply: JSONObject): BleSessionAccess {
            if (!reply.optBoolean("ok", false)) return BleSessionAccess()
            val capabilities = reply.optJSONObject("capabilities") ?: JSONObject()
            return BleSessionAccess(
                actor = reply.optString("actor").trim(),
                name = reply.optString("name").trim(),
                role = reply.optString("role").trim(),
                monitor = capabilities.optBoolean("monitor", false),
                armHome = capabilities.optBoolean("armHome", false),
                armAway = capabilities.optBoolean("armAway", false),
                disarm = capabilities.optBoolean("disarm", false),
                panic = capabilities.optBoolean("panic", false),
                valves = capabilities.optBoolean("valves", false),
                networkConfigure = capabilities.optBoolean("networkConfigure", false),
                accessManage = capabilities.optBoolean("accessManage", false),
                serviceInvalidate = capabilities.optBoolean("serviceInvalidate", false),
            )
        }
    }
}

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
    private val accessFlow = MutableStateFlow(BleSessionAccess())

    fun state(): StateFlow<BleHomeGuardClient.State> = client.state()
    fun snapshots(): StateFlow<SystemSnapshot> = client.snapshots()
    fun commandReplies(): StateFlow<JSONObject?> = client.commandReplies()
    fun access(): StateFlow<BleSessionAccess> = accessFlow.asStateFlow()
    fun isReady(): Boolean = client.state().value == BleHomeGuardClient.State.READY

    suspend fun connect(deviceId: String, timeoutMs: Long = 15_000L) {
        require(deviceId.isNotBlank()) { "BLE device id is empty" }
        if (client.state().value == BleHomeGuardClient.State.READY ||
            client.state().value == BleHomeGuardClient.State.CONNECTED) return

        accessFlow.value = BleSessionAccess()
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
            accessFlow.value = BleSessionAccess()
            "BLE authentication rejected: ${reply.optString("reason", "unauthorized")}"
        }
        accessFlow.value = BleSessionAccess.fromHello(reply)
        require(accessFlow.value.authenticated) { "BLE authentication reply has no actor" }
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
        if (isReady() && accessFlow.value.authenticated) {
            return JSONObject()
                .put("ok", true)
                .put("state", "authenticated")
                .put("actor", accessFlow.value.actor)
                .put("name", accessFlow.value.name)
                .put("role", accessFlow.value.role)
                .put("transport", "ble")
        }
        return authenticate(actor, pin, authTimeoutMs)
    }

    suspend fun execute(type: CommandType, timeoutMs: Long = 8_000L): JSONObject {
        require(isReady()) { "BLE runtime is not authenticated" }
        return when (type) {
            CommandType.ARM_HOME -> executeNamedCommand("security.arm_home", type, timeoutMs)
            CommandType.ARM_AWAY -> executeNamedCommand("security.arm_away", type, timeoutMs)
            CommandType.DISARM -> executeNamedCommand("security.disarm", type, timeoutMs)
            CommandType.OPEN_VALVES -> executeValves(active = true, timeoutMs = timeoutMs)
            CommandType.CLOSE_VALVES -> executeValves(active = false, timeoutMs = timeoutMs)
            else -> throw IllegalArgumentException("Command $type is not available over BLE runtime yet")
        }
    }

    private suspend fun executeNamedCommand(command: String, type: CommandType, timeoutMs: Long): JSONObject {
        requireAllowed(command)
        require(client.sendCommand(command)) { "Command $type could not start over BLE" }
        return awaitCommandReply(timeoutMs)
    }

    private suspend fun executeValves(active: Boolean, timeoutMs: Long): JSONObject {
        requireAllowed(if (active) "valve.open" else "valve.close")
        var lastReply = JSONObject().put("ok", true).put("status", "accepted")
        // Keep BLE valve semantics identical to runtime HTTP: valve outputs are 2 and 3.
        for (outputId in 2..3) {
            require(client.controlOutput(outputId, active, false)) {
                "BLE valve output $outputId command could not start"
            }
            val reply = awaitCommandReply(timeoutMs)
            if (!reply.optBoolean("ok", false)) return reply
            lastReply = reply
        }
        return lastReply
    }

    suspend fun panic(timeoutMs: Long = 8_000L): JSONObject {
        require(isReady()) { "BLE runtime is not authenticated" }
        requireAllowed("security.panic")
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
        requireAllowed(command)
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
        // Generic output.control does not have a matching HELLO capability yet;
        // the ESP access-control layer remains authoritative for this command.
        require(client.controlOutput(outputId, active, alarmActive)) { "BLE output command could not start" }
        return awaitCommandReply(timeoutMs)
    }

    fun disconnect() {
        accessFlow.value = BleSessionAccess()
        client.disconnect()
    }

    private fun requireAllowed(command: String) {
        val current = accessFlow.value
        require(current.authenticated) { "BLE session access metadata is unavailable" }
        require(current.allows(command)) {
            "BLE role ${current.role.ifBlank { "unknown" }} does not allow $command"
        }
    }

    private suspend fun awaitCommandReply(timeoutMs: Long): JSONObject = withTimeout(timeoutMs) {
        client.commandReplies().filterNotNull().first()
    }
}
