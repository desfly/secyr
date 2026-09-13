package ua.homeguard.s3.network.ble

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothProfile
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.json.JSONObject
import ua.homeguard.s3.model.ProvisioningForm
import ua.homeguard.s3.model.ProvisioningQrData
import ua.homeguard.s3.model.SystemSnapshot
import ua.homeguard.s3.model.Transport
import ua.homeguard.s3.network.JsonParsers
import java.util.ArrayDeque
import java.util.UUID
import java.util.concurrent.atomic.AtomicInteger

class BleHomeGuardClient(private val context: Context) {
    enum class State {
        IDLE, CONNECTING, DISCOVERING, SUBSCRIBING, CONNECTED,
        AUTHENTICATING, PROVISIONING, READY, OFFLINE, ERROR
    }

    private val decoder = BleFrameCodec.Decoder()
    private val stateFlow = MutableStateFlow(State.IDLE)
    private val snapshotFlow = MutableStateFlow(SystemSnapshot())
    private val commandReplyFlow = MutableStateFlow<JSONObject?>(null)
    private val sessionReplyFlow = MutableStateFlow<JSONObject?>(null)
    private val provisioningReplyFlow = MutableStateFlow<JSONObject?>(null)
    private val errorFlow = MutableStateFlow<JSONObject?>(null)
    private val writeQueue = ArrayDeque<ByteArray>()
    private val mainHandler = Handler(Looper.getMainLooper())
    private var writeInFlight = false
    @Volatile private var gatt: BluetoothGatt? = null
    @Volatile private var rx: BluetoothGattCharacteristic? = null
    @Volatile private var tx: BluetoothGattCharacteristic? = null
    @Volatile private var mtu = 23
    private val nextMessageId = AtomicInteger(1)
    private var sessionActor: String? = null
    private var pendingActor: String? = null
    private var securityProbeAttempt = 0
    private var securityProbeRunnable: Runnable? = null

    fun state(): StateFlow<State> = stateFlow.asStateFlow()
    fun snapshots(): StateFlow<SystemSnapshot> = snapshotFlow.asStateFlow()
    fun commandReplies(): StateFlow<JSONObject?> = commandReplyFlow.asStateFlow()
    fun sessionReplies(): StateFlow<JSONObject?> = sessionReplyFlow.asStateFlow()
    fun provisioningReplies(): StateFlow<JSONObject?> = provisioningReplyFlow.asStateFlow()
    fun errors(): StateFlow<JSONObject?> = errorFlow.asStateFlow()

    companion object {
        private val CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        // Pairing on some Android stacks takes several seconds even for Just Works.
        // Keep the probe window inside BleRuntimeSession's 12 second connection
        // timeout, but do not abort the link after the previous ~3 second window.
        private const val SECURITY_PROBE_MAX_ATTEMPTS = 12
        private const val SECURITY_PROBE_RETRY_MS = 750L
        private const val GATT_INSUFFICIENT_AUTHENTICATION = 5
        private const val GATT_INSUFFICIENT_ENCRYPTION = 15
        private const val GATT_ANDROID_GENERIC_ERROR = 133

        /**
         * HomeGuard frames have their own message-level replies and sequencing, so
         * prefer an ATT Write Command when the RX characteristic advertises it.
         * This avoids making the first HELLO notification compete with an ATT Write
         * Response on vendor stacks that surface that collision as generic 133.
         */
        internal fun preferredWriteType(properties: Int): Int? = when {
            properties and BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE != 0 ->
                BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
            properties and BluetoothGattCharacteristic.PROPERTY_WRITE != 0 ->
                BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            else -> null
        }

        fun runtimePermissions(): Array<String> = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        } else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
    }

    @SuppressLint("MissingPermission")
    fun connect(device: BluetoothDevice) {
        disconnect(resetDiagnostics = false)
        stateFlow.value = State.CONNECTING
        BleRuntimeDiagnostics.update("CONNECTING", "connectGatt", address = device.address)
        // Pin every GATT callback to one looper. Vendor stacks otherwise may
        // dispatch callbacks on different Binder threads and race teardown,
        // security probing and queued writes.
        gatt = device.connectGatt(
            context,
            false,
            callback,
            BluetoothDevice.TRANSPORT_LE,
            BluetoothDevice.PHY_LE_1M_MASK,
            mainHandler,
        )
        if (gatt == null) {
            stateFlow.value = State.ERROR
            BleRuntimeDiagnostics.update("CONNECT_ERROR", "connectGatt returned null", address = device.address)
        }
    }

    fun disconnect() = disconnect(resetDiagnostics = true)

    /**
     * Tears down a failed GATT attempt before the scanner looks for the same
     * peripheral again. Diagnostics intentionally survive so the retry remains
     * visible as one traceable connection chain.
     */
    internal fun disconnectForRetry() = disconnect(resetDiagnostics = false)

    @SuppressLint("MissingPermission")
    private fun disconnect(resetDiagnostics: Boolean) {
        cancelSecurityProbe()
        // Detach first so a late callback from this attempt cannot mutate the
        // next attempt. Keep a local reference so close() is guaranteed even if
        // disconnect() synchronously dispatches a vendor callback.
        val currentGatt = gatt
        gatt = null
        runCatching { currentGatt?.disconnect() }
        runCatching { currentGatt?.close() }
        rx = null; tx = null; mtu = 23; decoder.reset(); sessionActor = null; pendingActor = null
        synchronized(writeQueue) { writeQueue.clear(); writeInFlight = false }
        snapshotFlow.value = SystemSnapshot()
        commandReplyFlow.value = null
        sessionReplyFlow.value = null
        provisioningReplyFlow.value = null
        errorFlow.value = null
        stateFlow.value = State.IDLE
        if (resetDiagnostics) BleRuntimeDiagnostics.reset()
    }

    fun authenticate(actor: String, pin: String): Boolean {
        if (stateFlow.value != State.CONNECTED && stateFlow.value != State.READY) return false
        sessionActor = null
        pendingActor = actor
        sessionReplyFlow.value = null
        stateFlow.value = State.AUTHENTICATING
        BleRuntimeDiagnostics.update("AUTHENTICATING", "HELLO_SESSION actor=$actor")
        val queued = sendJson(
            HomeGuardBleContract.Type.HELLO_SESSION,
            JSONObject().put("actor", actor).put("pin", pin)
        )
        if (!queued) {
            pendingActor = null
            stateFlow.value = State.CONNECTED
            BleRuntimeDiagnostics.update("AUTH_QUEUE_ERROR", "HELLO_SESSION could not be queued")
        }
        return queued
    }

    /**
     * Starts factory commissioning over BLE. This deliberately does not use an
     * Admin PIN: a new/reset panel may not have an owner yet. The proof is the
     * same QR pairing code + certificate fingerprint used by HTTPS provisioning.
     */
    fun authorizeProvisioning(qr: ProvisioningQrData): Boolean {
        if (stateFlow.value != State.CONNECTED && stateFlow.value != State.PROVISIONING) return false
        provisioningReplyFlow.value = null
        stateFlow.value = State.PROVISIONING
        val queued = sendJson(
            HomeGuardBleContract.Type.PROVISIONING_AUTHORIZE,
            JSONObject()
                .put("pairing_code", qr.pairingCode)
                .put("certificate_sha256", qr.certificateSha256)
                .put("device_id", qr.deviceId)
        )
        if (!queued) stateFlow.value = State.CONNECTED
        return queued
    }

    /** Sends Wi-Fi/cloud ownership data only after a successful provisioning proof. */
    fun applyProvisioning(form: ProvisioningForm, localApiToken: String): Boolean {
        if (stateFlow.value != State.PROVISIONING) return false
        provisioningReplyFlow.value = null
        return sendJson(
            HomeGuardBleContract.Type.PROVISIONING_APPLY,
            JSONObject()
                .put("wifi_ssid", form.wifiSsid)
                .put("wifi_password", form.wifiPassword)
                .put("owner_label", form.ownerLabel)
                .put("cloud_endpoint", form.cloudEndpoint)
                .put("cloud_token", form.cloudClaimToken)
                .put("local_api_token", localApiToken)
        )
    }

    fun sendCommand(command: String, arguments: JSONObject = JSONObject()): Boolean {
        val actor = sessionActor ?: return false
        if (stateFlow.value != State.READY) return false
        val payload = JSONObject(arguments.toString())
            .put("actor", actor)
            .put("command", command)
        commandReplyFlow.value = null
        return sendJson(HomeGuardBleContract.Type.COMMAND, payload)
    }

    fun controlOutput(outputId: Int, active: Boolean, alarmActive: Boolean = false): Boolean =
        sendCommand(
            "output.control",
            JSONObject()
                .put("outputId", outputId)
                .put("active", active)
                .put("alarmActive", alarmActive)
        )

    fun armAway(): Boolean = sendCommand("security.arm_away")
    fun armHome(): Boolean = sendCommand("security.arm_home")
    fun disarm(): Boolean = sendCommand("security.disarm")
    fun panic(): Boolean = sendCommand("security.panic")

    fun sendJson(type: Int, json: JSONObject): Boolean {
        if (gatt == null || rx == null || stateFlow.value == State.IDLE || stateFlow.value == State.OFFLINE || stateFlow.value == State.ERROR) return false
        val id = nextMessageId.getAndIncrement() and 0xffff
        val frames = BleFrameCodec.encode(type, id, json.toString().toByteArray(Charsets.UTF_8), mtu)
        synchronized(writeQueue) {
            frames.forEach(writeQueue::addLast)
            if (!writeInFlight) return writeNextLocked()
        }
        return true
    }

    private val callback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            if (gatt !== g) return
            if (status == BluetoothGatt.GATT_SUCCESS && newState == BluetoothProfile.STATE_CONNECTED) {
                stateFlow.value = State.DISCOVERING
                BleRuntimeDiagnostics.update("DISCOVERING", "GATT connected; discoverServices", status)
                if (!g.discoverServices()) {
                    stateFlow.value = State.ERROR
                    BleRuntimeDiagnostics.update("DISCOVER_ERROR", "discoverServices returned false", status)
                }
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                cancelSecurityProbe()
                // A remotely disconnected BluetoothGatt must be closed as well.
                // Keeping it around leaks a client slot and is a common precursor
                // to status 133 on subsequent connection attempts.
                gatt = null
                runCatching { g.close() }
                rx = null; tx = null; decoder.reset(); sessionActor = null; pendingActor = null
                synchronized(writeQueue) { writeQueue.clear(); writeInFlight = false }
                snapshotFlow.value = SystemSnapshot(); stateFlow.value = State.OFFLINE
                BleRuntimeDiagnostics.update("DISCONNECTED", "GATT disconnected; newState=$newState", status)
            } else if (status != BluetoothGatt.GATT_SUCCESS) {
                stateFlow.value = State.ERROR
                BleRuntimeDiagnostics.update("CONNECT_ERROR", "onConnectionStateChange newState=$newState", status)
            }
        }

        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            if (gatt !== g) return
            if (status != BluetoothGatt.GATT_SUCCESS) {
                stateFlow.value = State.ERROR
                BleRuntimeDiagnostics.update("DISCOVER_ERROR", "onServicesDiscovered", status)
                return
            }
            val service = g.getService(HomeGuardBleContract.SERVICE_UUID)
            if (service == null) {
                stateFlow.value = State.ERROR
                BleRuntimeDiagnostics.update("SERVICE_MISSING", "HomeGuard service UUID not found", status)
                return
            }
            rx = service.getCharacteristic(HomeGuardBleContract.RX_UUID)
            tx = service.getCharacteristic(HomeGuardBleContract.TX_UUID)
            if (rx == null || tx == null) {
                stateFlow.value = State.ERROR
                BleRuntimeDiagnostics.update("CHARACTERISTIC_MISSING", "rx=${rx != null} tx=${tx != null}", status)
                return
            }
            stateFlow.value = State.SUBSCRIBING
            BleRuntimeDiagnostics.update("MTU_REQUEST", "request ${HomeGuardBleContract.PREFERRED_MTU}", status)
            if (!g.requestMtu(HomeGuardBleContract.PREFERRED_MTU)) {
                mtu = 23
                BleRuntimeDiagnostics.update("SUBSCRIBING", "MTU request rejected; fallback=23")
                subscribe(g)
            }
        }

        @SuppressLint("MissingPermission")
        override fun onMtuChanged(g: BluetoothGatt, newMtu: Int, status: Int) {
            if (gatt !== g) return
            mtu = if (status == BluetoothGatt.GATT_SUCCESS) newMtu else 23
            BleRuntimeDiagnostics.update("SUBSCRIBING", "MTU=$mtu; write CCCD", status)
            subscribe(g)
        }

        @Deprecated("Android 13 compatibility callback")
        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (gatt === g && characteristic.uuid == HomeGuardBleContract.TX_UUID) accept(characteristic.value ?: return)
        }

        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            if (gatt === g && characteristic.uuid == HomeGuardBleContract.TX_UUID) accept(value)
        }

        @Deprecated("Android 13 compatibility callback")
        override fun onCharacteristicRead(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (gatt === g && characteristic.uuid == HomeGuardBleContract.TX_UUID) completeSecurityProbe(g, status)
        }

        override fun onCharacteristicRead(
            g: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray,
            status: Int,
        ) {
            if (gatt === g && characteristic.uuid == HomeGuardBleContract.TX_UUID) completeSecurityProbe(g, status)
        }

        override fun onDescriptorWrite(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            if (gatt !== g || descriptor.uuid != CCCD) return
            if (status == BluetoothGatt.GATT_SUCCESS) {
                stateFlow.value = State.SUBSCRIBING
                BleRuntimeDiagnostics.update(
                    "SECURITY_WAIT",
                    "notifications enabled; verify encrypted ATT before CONNECTED",
                    status,
                )
                securityProbeAttempt = 0
                scheduleSecurityProbe(g, 250L)
            } else {
                stateFlow.value = State.ERROR
                BleRuntimeDiagnostics.update("SUBSCRIBE_ERROR", "CCCD write failed", status)
            }
        }

        override fun onCharacteristicWrite(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (gatt !== g || characteristic.uuid != HomeGuardBleContract.RX_UUID) return
            synchronized(writeQueue) {
                writeInFlight = false
                if (status == BluetoothGatt.GATT_SUCCESS) writeNextLocked()
                else {
                    writeQueue.clear(); stateFlow.value = State.ERROR
                    val type = preferredWriteType(characteristic.properties)
                    BleRuntimeDiagnostics.update(
                        "WRITE_ERROR",
                        "RX characteristic write failed; type=${if (type == BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE) "command" else "request"}; props=0x${characteristic.properties.toString(16)}",
                        status,
                    )
                }
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun subscribe(g: BluetoothGatt) {
        val characteristic = tx ?: run {
            stateFlow.value = State.ERROR
            BleRuntimeDiagnostics.update("SUBSCRIBE_ERROR", "TX characteristic unavailable")
            return
        }
        if (!g.setCharacteristicNotification(characteristic, true)) {
            stateFlow.value = State.ERROR
            BleRuntimeDiagnostics.update("SUBSCRIBE_ERROR", "setCharacteristicNotification returned false")
            return
        }
        val cccd = characteristic.getDescriptor(CCCD) ?: run {
            stateFlow.value = State.ERROR
            BleRuntimeDiagnostics.update("SUBSCRIBE_ERROR", "CCCD descriptor missing")
            return
        }
        val writeStarted = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            g.writeDescriptor(cccd, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) ==
                BluetoothGatt.GATT_SUCCESS
        } else {
            @Suppress("DEPRECATION")
            run {
                cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                g.writeDescriptor(cccd)
            }
        }
        if (!writeStarted) {
            stateFlow.value = State.ERROR
            BleRuntimeDiagnostics.update("SUBSCRIBE_ERROR", "writeDescriptor returned false")
        }
    }

    @SuppressLint("MissingPermission")
    private fun scheduleSecurityProbe(g: BluetoothGatt, delayMs: Long) {
        securityProbeRunnable?.let(mainHandler::removeCallbacks)
        val runnable = Runnable {
            if (gatt !== g || stateFlow.value == State.OFFLINE || stateFlow.value == State.ERROR) return@Runnable
            val characteristic = tx ?: run {
                stateFlow.value = State.ERROR
                BleRuntimeDiagnostics.update("SECURITY_ERROR", "TX characteristic unavailable for encrypted read")
                return@Runnable
            }
            securityProbeAttempt += 1
            BleRuntimeDiagnostics.update(
                "SECURITY_PROBE",
                "encrypted TX read attempt=$securityProbeAttempt/$SECURITY_PROBE_MAX_ATTEMPTS",
            )
            if (!g.readCharacteristic(characteristic)) {
                if (securityProbeAttempt < SECURITY_PROBE_MAX_ATTEMPTS) {
                    BleRuntimeDiagnostics.update("SECURITY_WAIT", "encrypted TX read could not start; retrying")
                    scheduleSecurityProbe(g, SECURITY_PROBE_RETRY_MS)
                } else {
                    stateFlow.value = State.ERROR
                    BleRuntimeDiagnostics.update("SECURITY_ERROR", "encrypted TX read could not start")
                }
            }
        }
        securityProbeRunnable = runnable
        mainHandler.postDelayed(runnable, delayMs)
    }

    private fun completeSecurityProbe(g: BluetoothGatt, status: Int) {
        if (gatt !== g) return
        if (status == BluetoothGatt.GATT_SUCCESS) {
            cancelSecurityProbe()
            stateFlow.value = State.CONNECTED
            BleRuntimeDiagnostics.update("SECURITY_READY", "encrypted TX read confirmed; GATT ready", status)
            return
        }

        val retryable = status == GATT_INSUFFICIENT_AUTHENTICATION ||
            status == GATT_INSUFFICIENT_ENCRYPTION ||
            status == GATT_ANDROID_GENERIC_ERROR
        if (retryable && securityProbeAttempt < SECURITY_PROBE_MAX_ATTEMPTS) {
            BleRuntimeDiagnostics.update("SECURITY_WAIT", "encrypted TX read not ready; retrying", status)
            scheduleSecurityProbe(g, SECURITY_PROBE_RETRY_MS)
            return
        }

        cancelSecurityProbe()
        stateFlow.value = State.ERROR
        BleRuntimeDiagnostics.update("SECURITY_ERROR", "encrypted TX read failed", status)
    }

    private fun cancelSecurityProbe() {
        securityProbeRunnable?.let(mainHandler::removeCallbacks)
        securityProbeRunnable = null
        securityProbeAttempt = 0
    }

    @SuppressLint("MissingPermission")
    private fun writeNextLocked(): Boolean {
        val currentGatt = gatt ?: return false
        val characteristic = rx ?: return false
        val frame = writeQueue.pollFirst() ?: return true
        val writeType = preferredWriteType(characteristic.properties) ?: run {
            writeQueue.clear()
            stateFlow.value = State.ERROR
            BleRuntimeDiagnostics.update(
                "WRITE_ERROR",
                "RX characteristic is not writable; props=0x${characteristic.properties.toString(16)}",
            )
            return false
        }
        writeInFlight = true
        BleRuntimeDiagnostics.update(
            "WRITE_START",
            "RX type=${if (writeType == BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE) "command" else "request"}; bytes=${frame.size}; props=0x${characteristic.properties.toString(16)}",
        )
        val writeStarted = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            currentGatt.writeCharacteristic(characteristic, frame, writeType) == BluetoothGatt.GATT_SUCCESS
        } else {
            @Suppress("DEPRECATION")
            run {
                characteristic.writeType = writeType
                characteristic.value = frame
                currentGatt.writeCharacteristic(characteristic)
            }
        }
        if (!writeStarted) {
            writeInFlight = false; writeQueue.clear(); stateFlow.value = State.ERROR
            BleRuntimeDiagnostics.update(
                "WRITE_ERROR",
                "writeCharacteristic could not start; type=${if (writeType == BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE) "command" else "request"}; props=0x${characteristic.properties.toString(16)}",
            )
            return false
        }
        return true
    }

    private fun accept(frame: ByteArray) {
        val message = runCatching { decoder.accept(frame) }.getOrElse {
            decoder.reset(); stateFlow.value = State.ERROR
            BleRuntimeDiagnostics.update("FRAME_ERROR", it.message ?: "frame decode failed")
            return
        } ?: return

        val json = runCatching { JSONObject(message.payload.toString(Charsets.UTF_8)) }.getOrElse {
            stateFlow.value = State.ERROR
            BleRuntimeDiagnostics.update("JSON_ERROR", it.message ?: "invalid BLE JSON")
            return
        }

        when (message.type) {
            HomeGuardBleContract.Type.TELEMETRY -> runCatching {
                val parsed = JsonParsers.snapshot(json)
                snapshotFlow.value = parsed.copy(transport = Transport.BLE)
            }.onFailure {
                stateFlow.value = State.ERROR
                BleRuntimeDiagnostics.update("TELEMETRY_ERROR", it.message ?: "telemetry parse failed")
            }

            HomeGuardBleContract.Type.HELLO_SESSION -> {
                sessionReplyFlow.value = json
                if (json.optBoolean("ok", false)) {
                    sessionActor = pendingActor
                    pendingActor = null
                    stateFlow.value = if (sessionActor != null) State.READY else State.CONNECTED
                    BleRuntimeDiagnostics.update(
                        if (sessionActor != null) "READY" else "CONNECTED",
                        if (sessionActor != null) "HELLO_SESSION accepted" else "HELLO reply missing pending actor",
                    )
                } else {
                    sessionActor = null
                    pendingActor = null
                    stateFlow.value = State.CONNECTED
                    BleRuntimeDiagnostics.update("AUTH_REJECTED", json.optString("reason", "unauthorized"))
                }
            }

            HomeGuardBleContract.Type.PROVISIONING_REPLY -> {
                provisioningReplyFlow.value = json
                when (json.optString("stage")) {
                    "authorized" -> stateFlow.value = State.PROVISIONING
                    "applied", "failed" -> stateFlow.value = State.CONNECTED
                    else -> if (!json.optBoolean("ok", false)) stateFlow.value = State.CONNECTED
                }
            }

            HomeGuardBleContract.Type.COMMAND_REPLY -> commandReplyFlow.value = json
            HomeGuardBleContract.Type.ERROR -> {
                errorFlow.value = json
                BleRuntimeDiagnostics.update("PROTOCOL_ERROR", json.optString("reason", "unknown"))
                if (json.optString("reason") == "ble_session_required") {
                    sessionActor = null
                    pendingActor = null
                    stateFlow.value = State.CONNECTED
                }
            }
        }
    }
}
