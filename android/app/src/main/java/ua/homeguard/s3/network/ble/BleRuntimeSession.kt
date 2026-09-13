package ua.homeguard.s3.network.ble

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import kotlinx.coroutines.NonCancellable
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.filter
import kotlinx.coroutines.flow.filterNotNull
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeout
import org.json.JSONObject
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.SystemSnapshot
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

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
    private val accessFlow = MutableStateFlow(BleSessionAccess())

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

    suspend fun connect(deviceId: String, timeoutMs: Long = 15_000L) {
        require(deviceId.isNotBlank()) { "BLE device id is empty" }
        if (client.state().value == BleHomeGuardClient.State.READY) return

        accessFlow.value = BleSessionAccess()
        val effectiveConnectTimeoutMs = timeoutMs.coerceAtLeast(12_000L)
        var connectedDevice: BluetoothDevice? = null
        var lastFailure: Throwable? = null

        for (attempt in 1..2) {
            try {
                val device = scanner.find(deviceId, effectiveConnectTimeoutMs)
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
                            else -> false
                        }
                    }.first()
                }
                connectedDevice = device
                lastFailure = null
                break
            } catch (failure: Throwable) {
                lastFailure = failure
                if (attempt < 2) {
                    BleRuntimeDiagnostics.update(
                        stage = "RETRYING",
                        detail = "early GATT drop; retrying once after 750ms",
                    )
                    delay(750L)
                }
            }
        }

        val device = connectedDevice ?: throw (lastFailure ?: IllegalStateException("BLE connection failed"))

        // ESP RX/TX characteristics require encrypted ATT. The controller now
        // initiates SMP on the already-live GATT link after CCCD subscription;
        // Android only observes the resulting bond state and never races it
        // with BluetoothDevice.createBond().
        awaitEspInitiatedBond(device, effectiveConnectTimeoutMs)
        require(
            client.state().value == BleHomeGuardClient.State.CONNECTED ||
                client.state().value == BleHomeGuardClient.State.READY
        ) { "BLE link dropped during post-GATT bonding: ${client.state().value}" }
    }

    @SuppressLint("MissingPermission")
    private suspend fun awaitEspInitiatedBond(device: BluetoothDevice, timeoutMs: Long) {
        if (device.bondState == BluetoothDevice.BOND_BONDED) {
            BleRuntimeDiagnostics.update(
                stage = "BOND_READY",
                detail = "already bonded",
                address = device.address,
            )
            return
        }

        BleRuntimeDiagnostics.update(
            stage = "SECURITY_WAIT",
            detail = "waiting for ESP-initiated pairing on active GATT",
            address = device.address,
        )

        withTimeout(timeoutMs.coerceAtLeast(12_000L)) {
            suspendCancellableCoroutine { continuation ->
                var receiverRegistered = true
                lateinit var bondReceiver: BroadcastReceiver

                fun unregister() {
                    if (!receiverRegistered) return
                    receiverRegistered = false
                    runCatching { appContext.unregisterReceiver(bondReceiver) }
                }

                val filter = IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED)
                bondReceiver = object : BroadcastReceiver() {
                    override fun onReceive(context: Context?, intent: Intent?) {
                        if (intent?.action != BluetoothDevice.ACTION_BOND_STATE_CHANGED) return
                        val changedDevice = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                            intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java)
                        } else {
                            @Suppress("DEPRECATION")
                            intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE) as? BluetoothDevice
                        } ?: return
                        if (!changedDevice.address.equals(device.address, ignoreCase = true)) return

                        val state = intent.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, BluetoothDevice.ERROR)
                        val previous = intent.getIntExtra(BluetoothDevice.EXTRA_PREVIOUS_BOND_STATE, BluetoothDevice.ERROR)
                        when (state) {
                            BluetoothDevice.BOND_BONDING -> {
                                BleRuntimeDiagnostics.update(
                                    stage = "BONDING",
                                    detail = "ESP-initiated pairing in progress",
                                    address = device.address,
                                )
                            }
                            BluetoothDevice.BOND_BONDED -> {
                                BleRuntimeDiagnostics.update(
                                    stage = "BOND_READY",
                                    detail = "encrypted bond established on active GATT",
                                    address = device.address,
                                )
                                unregister()
                                if (continuation.isActive) continuation.resume(Unit)
                            }
                            BluetoothDevice.BOND_NONE -> if (previous == BluetoothDevice.BOND_BONDING) {
                                BleRuntimeDiagnostics.update(
                                    stage = "BOND_ERROR",
                                    detail = "ESP-initiated pairing rejected or failed",
                                    address = device.address,
                                )
                                unregister()
                                if (continuation.isActive) {
                                    continuation.resumeWithException(
                                        IllegalStateException("BLE bonding failed for ${device.address}")
                                    )
                                }
                            }
                        }
                    }
                }

                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    appContext.registerReceiver(bondReceiver, filter, Context.RECEIVER_NOT_EXPORTED)
                } else {
                    @Suppress("DEPRECATION")
                    appContext.registerReceiver(bondReceiver, filter)
                }

                continuation.invokeOnCancellation { unregister() }

                // Pairing can finish between CONNECTED and receiver registration.
                // Re-read bondState, but deliberately never call createBond(): the
                // ESP owns SMP initiation for this encrypted GATT transport.
                if (device.bondState == BluetoothDevice.BOND_BONDED) {
                    BleRuntimeDiagnostics.update("BOND_READY", "already bonded", address = device.address)
                    unregister()
                    if (continuation.isActive) continuation.resume(Unit)
                } else if (device.bondState == BluetoothDevice.BOND_BONDING) {
                    BleRuntimeDiagnostics.update(
                        stage = "BONDING",
                        detail = "ESP-initiated pairing already in progress",
                        address = device.address,
                    )
                }
            }
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
            "BLE authentication rejected: ${reply.optString("reason", "unauthorized") }"
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
        BleRuntimeDiagnostics.update(
            stage = "AUTH_SETTLE",
            detail = "waiting for ESP notification subscription",
        )
        delay(400L)
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

    suspend fun setLight(active: Boolean, timeoutMs: Long = 8_000L): JSONObject =
        controlOutput(LIGHT_OUTPUT_ID, active, false, timeoutMs)

    suspend fun setValve1(active: Boolean, timeoutMs: Long = 8_000L): JSONObject {
        requireAllowed(if (active) "valve.open" else "valve.close")
        return controlOutput(VALVE_1_OUTPUT_ID, active, false, timeoutMs)
    }

    suspend fun setValve2(active: Boolean, timeoutMs: Long = 8_000L): JSONObject {
        requireAllowed(if (active) "valve.open" else "valve.close")
        return controlOutput(VALVE_2_OUTPUT_ID, active, false, timeoutMs)
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
