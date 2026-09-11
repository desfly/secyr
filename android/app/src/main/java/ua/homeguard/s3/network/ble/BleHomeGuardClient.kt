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
    private var writeInFlight = false
    private var gatt: BluetoothGatt? = null
    private var rx: BluetoothGattCharacteristic? = null
    private var tx: BluetoothGattCharacteristic? = null
    private var mtu = 23
    private var nextMessageId = 1
    private var sessionActor: String? = null
    private var pendingActor: String? = null

    fun state(): StateFlow<State> = stateFlow.asStateFlow()
    fun snapshots(): StateFlow<SystemSnapshot> = snapshotFlow.asStateFlow()
    fun commandReplies(): StateFlow<JSONObject?> = commandReplyFlow.asStateFlow()
    fun sessionReplies(): StateFlow<JSONObject?> = sessionReplyFlow.asStateFlow()
    fun provisioningReplies(): StateFlow<JSONObject?> = provisioningReplyFlow.asStateFlow()
    fun errors(): StateFlow<JSONObject?> = errorFlow.asStateFlow()

    companion object {
        private val CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
        fun runtimePermissions(): Array<String> = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        } else arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
    }

    @SuppressLint("MissingPermission")
    fun connect(device: BluetoothDevice) {
        disconnect()
        stateFlow.value = State.CONNECTING
        gatt = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
    }

    @SuppressLint("MissingPermission")
    fun disconnect() {
        gatt?.disconnect(); gatt?.close(); gatt = null
        rx = null; tx = null; mtu = 23; decoder.reset(); sessionActor = null; pendingActor = null
        synchronized(writeQueue) { writeQueue.clear(); writeInFlight = false }
        snapshotFlow.value = SystemSnapshot()
        commandReplyFlow.value = null
        sessionReplyFlow.value = null
        provisioningReplyFlow.value = null
        errorFlow.value = null
        stateFlow.value = State.IDLE
    }

    fun authenticate(actor: String, pin: String): Boolean {
        if (stateFlow.value != State.CONNECTED && stateFlow.value != State.READY) return false
        sessionActor = null
        pendingActor = actor
        sessionReplyFlow.value = null
        stateFlow.value = State.AUTHENTICATING
        val queued = sendJson(
            HomeGuardBleContract.Type.HELLO_SESSION,
            JSONObject().put("actor", actor).put("pin", pin)
        )
        if (!queued) {
            pendingActor = null
            stateFlow.value = State.CONNECTED
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
        val id = nextMessageId++ and 0xffff
        val frames = BleFrameCodec.encode(type, id, json.toString().toByteArray(Charsets.UTF_8), mtu)
        synchronized(writeQueue) {
            frames.forEach(writeQueue::addLast)
            if (!writeInFlight) writeNextLocked()
        }
        return true
    }

    private val callback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            if (gatt !== g) return
            if (status == BluetoothGatt.GATT_SUCCESS && newState == BluetoothProfile.STATE_CONNECTED) {
                stateFlow.value = State.DISCOVERING; g.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                rx = null; tx = null; decoder.reset(); sessionActor = null; pendingActor = null
                synchronized(writeQueue) { writeQueue.clear(); writeInFlight = false }
                snapshotFlow.value = SystemSnapshot(); stateFlow.value = State.OFFLINE
            } else if (status != BluetoothGatt.GATT_SUCCESS) stateFlow.value = State.ERROR
        }

        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            if (gatt !== g || status != BluetoothGatt.GATT_SUCCESS) { stateFlow.value = State.ERROR; return }
            val service = g.getService(HomeGuardBleContract.SERVICE_UUID)
            rx = service?.getCharacteristic(HomeGuardBleContract.RX_UUID)
            tx = service?.getCharacteristic(HomeGuardBleContract.TX_UUID)
            if (rx == null || tx == null) { stateFlow.value = State.ERROR; return }
            stateFlow.value = State.SUBSCRIBING
            if (!g.requestMtu(HomeGuardBleContract.PREFERRED_MTU)) { mtu = 23; subscribe(g) }
        }

        @SuppressLint("MissingPermission")
        override fun onMtuChanged(g: BluetoothGatt, newMtu: Int, status: Int) {
            if (gatt !== g) return
            mtu = if (status == BluetoothGatt.GATT_SUCCESS) newMtu else 23
            subscribe(g)
        }

        @Deprecated("Android 13 compatibility callback")
        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            if (gatt === g && characteristic.uuid == HomeGuardBleContract.TX_UUID) accept(characteristic.value ?: return)
        }

        override fun onCharacteristicChanged(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            if (gatt === g && characteristic.uuid == HomeGuardBleContract.TX_UUID) accept(value)
        }

        override fun onDescriptorWrite(g: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            if (gatt !== g || descriptor.uuid != CCCD) return
            stateFlow.value = if (status == BluetoothGatt.GATT_SUCCESS) State.CONNECTED else State.ERROR
        }

        override fun onCharacteristicWrite(g: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (gatt !== g || characteristic.uuid != HomeGuardBleContract.RX_UUID) return
            synchronized(writeQueue) {
                writeInFlight = false
                if (status == BluetoothGatt.GATT_SUCCESS) writeNextLocked()
                else { writeQueue.clear(); stateFlow.value = State.ERROR }
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun subscribe(g: BluetoothGatt) {
        val characteristic = tx ?: return
        if (!g.setCharacteristicNotification(characteristic, true)) { stateFlow.value = State.ERROR; return }
        val cccd = characteristic.getDescriptor(CCCD) ?: run { stateFlow.value = State.ERROR; return }
        cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
        if (!g.writeDescriptor(cccd)) stateFlow.value = State.ERROR
    }

    @SuppressLint("MissingPermission")
    private fun writeNextLocked() {
        val currentGatt = gatt ?: return
        val characteristic = rx ?: return
        val frame = writeQueue.pollFirst() ?: return
        characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        characteristic.value = frame
        writeInFlight = true
        if (!currentGatt.writeCharacteristic(characteristic)) {
            writeInFlight = false; writeQueue.clear(); stateFlow.value = State.ERROR
        }
    }

    private fun accept(frame: ByteArray) {
        val message = runCatching { decoder.accept(frame) }.getOrElse {
            decoder.reset(); stateFlow.value = State.ERROR; return
        } ?: return

        val json = runCatching { JSONObject(message.payload.toString(Charsets.UTF_8)) }.getOrElse {
            stateFlow.value = State.ERROR; return
        }

        when (message.type) {
            HomeGuardBleContract.Type.TELEMETRY -> runCatching {
                val parsed = JsonParsers.snapshot(json)
                snapshotFlow.value = parsed.copy(transport = Transport.BLE)
            }.onFailure { stateFlow.value = State.ERROR }

            HomeGuardBleContract.Type.HELLO_SESSION -> {
                sessionReplyFlow.value = json
                if (json.optBoolean("ok", false)) {
                    sessionActor = pendingActor
                    pendingActor = null
                    stateFlow.value = if (sessionActor != null) State.READY else State.CONNECTED
                } else {
                    sessionActor = null
                    pendingActor = null
                    stateFlow.value = State.CONNECTED
                }
            }

            HomeGuardBleContract.Type.PROVISIONING_REPLY -> {
                provisioningReplyFlow.value = json
                // Authorization keeps the link in provisioning mode so the next
                // packet can carry the configuration. A completed/failed apply
                // returns control to the normal connected state; the ESP may then
                // restart or associate to the supplied Wi-Fi network.
                when (json.optString("stage")) {
                    "authorized" -> stateFlow.value = State.PROVISIONING
                    "applied", "failed" -> stateFlow.value = State.CONNECTED
                    else -> if (!json.optBoolean("ok", false)) stateFlow.value = State.CONNECTED
                }
            }

            HomeGuardBleContract.Type.COMMAND_REPLY -> commandReplyFlow.value = json
            HomeGuardBleContract.Type.ERROR -> {
                errorFlow.value = json
                if (json.optString("reason") == "ble_session_required") {
                    sessionActor = null
                    pendingActor = null
                    stateFlow.value = State.CONNECTED
                }
            }
        }
    }
}
