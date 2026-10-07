package ua.homeguard.s3.network.ble

import android.bluetooth.BluetoothDevice
import android.content.Context
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.filterNotNull
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeout
import org.json.JSONObject
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.SystemSnapshot
import java.util.concurrent.atomic.AtomicLong

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
        else -> true
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

class BleRuntimeSession(context: Context) {
    private val appContext = context.applicationContext
    private val scanner = BleProvisioningScanner(appContext)
    private val client = BleHomeGuardClient(appContext)
    private val connectRetrier = BleConnectRetrier()
    private val operationMutex = Mutex()
    private val lifecycleGeneration = AtomicLong(0L)
    private val accessFlow = MutableStateFlow(BleSessionAccess())
    @Volatile private var connectedDeviceId: String = ""

    companion object {
        const val VALVE_1_OUTPUT_ID = 2
        const val VALVE_2_OUTPUT_ID = 3
        const val LIGHT_OUTPUT_ID = 4
        const val LOCK_OUTPUT_ID = 5
        const val LOCK_PULSE_MS = 5_000L
    }

    fun state(): StateFlow<BleHomeGuardClient.State> = client.state()
    fun snapshots(): StateFlow<SystemSnapshot> = client.snapshots()
    fun commandReplies(): StateFlow<JSONObject?> = client.commandReplies()
    fun access(): StateFlow<BleSessionAccess> = accessFlow.asStateFlow()
    fun isReady(): Boolean = client.state().value == BleHomeGuardClient.State.READY

    suspend fun connect(deviceId: String, timeoutMs: Long = 15_000L) = operationMutex.withLock {
        connectUnlocked(deviceId, timeoutMs)
    }

    private suspend fun connectUnlocked(deviceId: String, timeoutMs: Long) {
        val normalizedDeviceId = deviceId.trim()
        require(normalizedDeviceId.isNotBlank()) { "BLE device id is empty" }
        val currentState = client.state().value
        if ((currentState == BleHomeGuardClient.State.CONNECTED || currentState == BleHomeGuardClient.State.READY) &&
            connectedDeviceId.equals(normalizedDeviceId, ignoreCase = true)
        ) return

        accessFlow.value = BleSessionAccess()
        connectedDeviceId = ""
        val generation = lifecycleGeneration.incrementAndGet()
        // A previous READY/ERROR/AUTHENTICATING link also suppresses peripheral
        // advertising. Always tear it down before the first scan of a new chain.
        client.disconnect()
        val effectiveConnectTimeoutMs = timeoutMs.coerceAtLeast(12_000L)
        val device = connectRetrier.run(
            attempt = { attempt ->
                ensureCurrentGeneration(generation)
                val device = scanner.find(normalizedDeviceId, effectiveConnectTimeoutMs)
                ensureCurrentGeneration(generation)
                BleRuntimeDiagnostics.update(
                    stage = "CONNECTING",
                    detail = "attempt=$attempt/2; connect GATT before encrypted bond",
                    address = device.address,
                )

                client.connect(device)
                withTimeout(effectiveConnectTimeoutMs) {
                    client.state().filter { state ->
                        when (state) {
                            BleHomeGuardClient.State.CONNECTED,
                            BleHomeGuardClient.State.READY -> true
                            BleHomeGuardClient.State.ERROR,
                            BleHomeGuardClient.State.OFFLINE -> throw IllegalStateException("BLE connection failed: $state")
                            BleHomeGuardClient.State.IDLE -> throw CancellationException("BLE connection cancelled")
                            else -> false
                        }
                    }.first()
                }
                ensureCurrentGeneration(generation)
                device
            },
            cleanupAfterFailure = {
                // The old connection suppresses ESP advertising. Closing it
                // before scanner.find() is therefore mandatory, not optional.
                client.disconnectForRetry()
            },
            onRetry = { nextAttempt, failure ->
                BleRuntimeDiagnostics.update(
                    stage = "RETRYING",
                    detail = "GATT attempt failed (${failure.javaClass.simpleName}); " +
                        "link closed before attempt=$nextAttempt/2 scan",
                )
            },
        )

        // BleHomeGuardClient now reports CONNECTED only after an encrypted read
        // of the ESP TX characteristic succeeds. That encrypted ATT probe is the
        // authoritative runtime security gate. Android's bondState broadcast can
        // lag behind encryption (and has been observed to remain BOND_BONDING for
        // >12s after SECURITY_READY), so do not block HELLO_SESSION on it.
        when (device.bondState) {
            BluetoothDevice.BOND_BONDED -> BleRuntimeDiagnostics.update(
                stage = "BOND_READY",
                detail = "encrypted ATT ready; persistent bond already complete",
                address = device.address,
            )
            BluetoothDevice.BOND_BONDING -> BleRuntimeDiagnostics.update(
                stage = "BOND_BACKGROUND",
                detail = "encrypted ATT ready; Android bond completion continues in background",
                address = device.address,
            )
            else -> BleRuntimeDiagnostics.update(
                stage = "BOND_BACKGROUND",
                detail = "encrypted ATT ready; bond state=${device.bondState}; continue runtime session",
                address = device.address,
            )
        }
        require(
            client.state().value == BleHomeGuardClient.State.CONNECTED ||
                client.state().value == BleHomeGuardClient.State.READY
        ) { "BLE encrypted ATT link dropped before authentication: ${client.state().value}" }
        ensureCurrentGeneration(generation)
        connectedDeviceId = normalizedDeviceId
    }

    suspend fun authenticate(actor: String, pin: String, timeoutMs: Long = 8_000L): JSONObject =
        operationMutex.withLock { authenticateUnlocked(actor, pin, timeoutMs) }

    private suspend fun authenticateUnlocked(actor: String, pin: String, timeoutMs: Long): JSONObject {
        require(actor.isNotBlank()) { "BLE actor is empty" }
        require(pin.length in 4..12 && pin.all(Char::isDigit)) { "Invalid BLE PIN" }
        try {
            require(client.authenticate(actor, pin)) { "BLE authentication could not start" }
            val reply = withTimeout(timeoutMs) {
                client.sessionReplies().filterNotNull().first()
            }
            require(reply.optBoolean("ok", false)) {
                "BLE authentication rejected: ${reply.optString("reason", "unauthorized") }"
            }
            accessFlow.value = BleSessionAccess.fromHello(reply)
            require(accessFlow.value.authenticated) { "BLE authentication reply has no actor" }
            withTimeout(timeoutMs) {
                client.state().filter { it == BleHomeGuardClient.State.READY }.first()
            }
            return reply
        } catch (failure: Throwable) {
            // A timed-out HELLO reply may still arrive later. Closing this link
            // prevents it from authenticating or satisfying a later operation.
            accessFlow.value = BleSessionAccess()
            invalidateSession()
            throw failure
        }
    }

    suspend fun connectAndAuthenticate(
        deviceId: String,
        actor: String,
        pin: String,
        connectTimeoutMs: Long = 15_000L,
        authTimeoutMs: Long = 8_000L,
    ): JSONObject = operationMutex.withLock {
        connectUnlocked(deviceId, connectTimeoutMs)
        // CONNECTED is emitted only after the CCCD callback and encrypted ATT
        // probe, so an arbitrary settling delay is both unnecessary and racy.
        authenticateUnlocked(actor, pin, authTimeoutMs)
    }

    suspend fun execute(type: CommandType, timeoutMs: Long = 8_000L): JSONObject = operationMutex.withLock {
        require(isReady()) { "BLE runtime is not authenticated" }
        when (type) {
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
        for (outputId in VALVE_1_OUTPUT_ID..VALVE_2_OUTPUT_ID) {
            require(client.controlOutput(outputId, active, false)) {
                "BLE valve output $outputId command could not start"
            }
            val reply = awaitCommandReply(timeoutMs)
            if (!reply.optBoolean("ok", false)) return reply
            lastReply = reply
        }
        return lastReply
    }

    suspend fun setLight(active: Boolean, timeoutMs: Long = 8_000L): JSONObject = operationMutex.withLock {
        controlOutputUnlocked(LIGHT_OUTPUT_ID, active, false, timeoutMs)
    }

    suspend fun setValve1(active: Boolean, timeoutMs: Long = 8_000L): JSONObject = operationMutex.withLock {
        requireAllowed(if (active) "valve.open" else "valve.close")
        controlOutputUnlocked(VALVE_1_OUTPUT_ID, active, false, timeoutMs)
    }

    suspend fun setValve2(active: Boolean, timeoutMs: Long = 8_000L): JSONObject = operationMutex.withLock {
        requireAllowed(if (active) "valve.open" else "valve.close")
        controlOutputUnlocked(VALVE_2_OUTPUT_ID, active, false, timeoutMs)
    }

    suspend fun pulseLock(timeoutMs: Long = 8_000L): JSONObject {
        val onReply = controlOutput(LOCK_OUTPUT_ID, true, false, timeoutMs)
        if (!onReply.optBoolean("ok", false)) return onReply
        try {
            delay(LOCK_PULSE_MS)
        } finally {
            withContext(NonCancellable) {
                runCatching { controlOutput(LOCK_OUTPUT_ID, false, false, timeoutMs) }
            }
        }
        return JSONObject(onReply.toString())
            .put("pulseMs", LOCK_PULSE_MS)
            .put("active", false)
    }

    suspend fun panic(timeoutMs: Long = 8_000L): JSONObject = operationMutex.withLock {
        require(isReady()) { "BLE runtime is not authenticated" }
        requireAllowed("security.panic")
        require(client.panic()) { "BLE panic command could not start" }
        awaitCommandReply(timeoutMs)
    }

    suspend fun sendCommand(
        command: String,
        arguments: JSONObject = JSONObject(),
        timeoutMs: Long = 8_000L,
    ): JSONObject = operationMutex.withLock {
        require(isReady()) { "BLE runtime is not authenticated" }
        require(command.isNotBlank()) { "BLE command is empty" }
        requireAllowed(command)
        require(client.sendCommand(command, arguments)) { "BLE command could not start" }
        awaitCommandReply(timeoutMs)
    }

    suspend fun controlOutput(
        outputId: Int,
        active: Boolean,
        alarmActive: Boolean = false,
        timeoutMs: Long = 8_000L,
    ): JSONObject = operationMutex.withLock {
        controlOutputUnlocked(outputId, active, alarmActive, timeoutMs)
    }

    private suspend fun controlOutputUnlocked(
        outputId: Int,
        active: Boolean,
        alarmActive: Boolean,
        timeoutMs: Long,
    ): JSONObject {
        require(isReady()) { "BLE runtime is not authenticated" }
        require(outputId in 1..65535) { "Invalid output id" }
        require(client.controlOutput(outputId, active, alarmActive)) { "BLE output command could not start" }
        return awaitCommandReply(timeoutMs)
    }

    fun disconnect() {
        lifecycleGeneration.incrementAndGet()
        connectedDeviceId = ""
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

    private suspend fun awaitCommandReply(timeoutMs: Long): JSONObject {
        try {
            return withTimeout(timeoutMs) {
                client.commandReplies().filterNotNull().first()
            }
        } catch (cancelled: CancellationException) {
            // Without request correlation, a late response must never be allowed
            // to satisfy the next command. Invalidate the whole GATT session.
            invalidateSession()
            throw cancelled
        }
    }

    private fun ensureCurrentGeneration(expected: Long) {
        if (lifecycleGeneration.get() != expected) {
            throw CancellationException("BLE connection superseded or cancelled")
        }
    }

    private fun invalidateSession() {
        lifecycleGeneration.incrementAndGet()
        connectedDeviceId = ""
        accessFlow.value = BleSessionAccess()
        client.disconnectForRetry()
    }
}
