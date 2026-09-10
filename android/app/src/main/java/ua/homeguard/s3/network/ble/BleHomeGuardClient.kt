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
import ua.homeguard.s3.model.SystemSnapshot
import ua.homeguard.s3.model.Transport
import ua.homeguard.s3.network.JsonParsers
import java.util.UUID

class BleHomeGuardClient(private val context: Context) {
    enum class State { IDLE, CONNECTING, DISCOVERING, SUBSCRIBING, CONNECTED, OFFLINE, ERROR }

    private val decoder = BleFrameCodec.Decoder()
    private val stateFlow = MutableStateFlow(State.IDLE)
    private val snapshotFlow = MutableStateFlow(SystemSnapshot())
    private var gatt: BluetoothGatt? = null
    private var rx: BluetoothGattCharacteristic? = null
    private var tx: BluetoothGattCharacteristic? = null
    private var mtu = 23
    private var nextMessageId = 1

    fun state(): StateFlow<State> = stateFlow.asStateFlow()
    fun snapshots(): StateFlow<SystemSnapshot> = snapshotFlow.asStateFlow()

    companion object {
        private val CCCD = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

        fun runtimePermissions(): Array<String> = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }
    }

    @SuppressLint("MissingPermission")
    fun connect(device: BluetoothDevice) {
        disconnect()
        stateFlow.value = State.CONNECTING
        gatt = device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE)
    }

    @SuppressLint("MissingPermission")
    fun disconnect() {
        gatt?.disconnect()
        gatt?.close()
        gatt = null
        rx = null
        tx = null
        mtu = 23
        decoder.reset()
        snapshotFlow.value = SystemSnapshot()
        stateFlow.value = State.IDLE
    }

    @SuppressLint("MissingPermission")
    fun sendJson(type: Int, json: JSONObject): Boolean {
        val currentGatt = gatt ?: return false
        val characteristic = rx ?: return false
        val id = nextMessageId++ and 0xffff
        val frames = BleFrameCodec.encode(type, id, json.toString().toByteArray(Charsets.UTF_8), mtu)
        // Stage-1 uses acknowledged writes to preserve fragment ordering. A queued async writer
        // will replace this when command/reply routing is connected to the full app controller.
        return frames.all { frame ->
            characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
            characteristic.value = frame
            currentGatt.writeCharacteristic(characteristic)
        }
    }

    private val callback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, newState: Int) {
            if (gatt !== g) return
            if (status == BluetoothGatt.GATT_SUCCESS && newState == BluetoothProfile.STATE_CONNECTED) {
                stateFlow.value = State.DISCOVERING
                g.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                rx = null; tx = null; decoder.reset(); snapshotFlow.value = SystemSnapshot()
                stateFlow.value = State.OFFLINE
            } else if (status != BluetoothGatt.GATT_SUCCESS) {
                stateFlow.value = State.ERROR
            }
        }

        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            if (gatt !== g || status != BluetoothGatt.GATT_SUCCESS) { stateFlow.value = State.ERROR; return }
            val service = g.getService(HomeGuardBleContract.SERVICE_UUID)
            rx = service?.getCharacteristic(HomeGuardBleContract.RX_UUID)
            tx = service?.getCharacteristic(HomeGuardBleContract.TX_UUID)
            if (rx == null || tx == null) { stateFlow.value = State.ERROR; return }
            stateFlow.value = State.SUBSCRIBING
            g.requestMtu(HomeGuardBleContract.PREFERRED_MTU)
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
    }

    @SuppressLint("MissingPermission")
    private fun subscribe(g: BluetoothGatt) {
        val characteristic = tx ?: return
        if (!g.setCharacteristicNotification(characteristic, true)) { stateFlow.value = State.ERROR; return }
        val cccd = characteristic.getDescriptor(CCCD) ?: run { stateFlow.value = State.ERROR; return }
        cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
        if (!g.writeDescriptor(cccd)) stateFlow.value = State.ERROR
    }

    private fun accept(frame: ByteArray) {
        val message = runCatching { decoder.accept(frame) }.getOrElse { decoder.reset(); stateFlow.value = State.ERROR; return }
        if (message?.type != HomeGuardBleContract.Type.TELEMETRY) return
        runCatching {
            val parsed = JsonParsers.snapshot(JSONObject(message.payload.toString(Charsets.UTF_8)))
            // A frame can describe the controller's IP transport; while this stream is active the
            // Android-facing transport is BLE, so diagnostics/UI must not stay NONE.
            snapshotFlow.value = parsed.copy(transport = Transport.BLE)
        }.onFailure { stateFlow.value = State.ERROR }
    }
}
