package ua.homeguard.s3.provisioning

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattService
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.ParcelUuid
import androidx.core.content.ContextCompat
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.withContext
import kotlinx.coroutines.withTimeout
import org.json.JSONObject
import ua.homeguard.s3.model.ProvisioningForm
import ua.homeguard.s3.model.ProvisioningQrData
import java.util.UUID
import java.util.concurrent.atomic.AtomicInteger

/**
 * Secure BLE provisioning transport for a factory-new HomeGuard-S3.
 *
 * The BLE link only transports the existing provisioning proof/payload. The
 * controller still validates the factory pairing code + certificate fingerprint
 * before accepting Wi-Fi or API credentials.
 */
class BleProvisioningClient(private val context: Context) : AutoCloseable {
    data class Reply(val ok: Boolean, val stage: String, val reason: String)

    companion object {
        private val SERVICE_UUID = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
        private val RX_UUID = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
        private val TX_UUID = UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e")
        private val CCCD_UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

        private const val PROTOCOL_VERSION: Byte = 1
        private const val HEADER_SIZE = 6
        private const val AUTHORIZE_TYPE: Byte = 8
        private const val APPLY_TYPE: Byte = 9
        private const val REPLY_TYPE: Byte = 10
    }

    private val manager = context.getSystemService(BluetoothManager::class.java)
    private val messageId = AtomicInteger(1)
    private val incoming = Channel<Pair<Byte, String>>(Channel.BUFFERED)

    private var gatt: BluetoothGatt? = null
    private var rx: BluetoothGattCharacteristic? = null
    private var tx: BluetoothGattCharacteristic? = null
    private var pendingConnect: CompletableDeferred<Unit>? = null
    private var pendingWrite: CompletableDeferred<Unit>? = null

    private var rxType: Byte = 0
    private var rxMessageId = -1
    private var rxExpectedCount = 0
    private var rxNextIndex = 0
    private val rxPayload = ArrayList<Byte>()

    private val callback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS || newState != BluetoothProfile.STATE_CONNECTED) {
                pendingConnect?.completeExceptionally(IllegalStateException("BLE connect failed: status=$status state=$newState"))
                return
            }
            if (!gatt.discoverServices()) {
                pendingConnect?.completeExceptionally(IllegalStateException("BLE service discovery did not start"))
            }
        }

        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                pendingConnect?.completeExceptionally(IllegalStateException("BLE service discovery failed: $status"))
                return
            }
            val service: BluetoothGattService = gatt.getService(SERVICE_UUID)
                ?: run {
                    pendingConnect?.completeExceptionally(IllegalStateException("HomeGuard BLE service not found"))
                    return
                }
            rx = service.getCharacteristic(RX_UUID)
            tx = service.getCharacteristic(TX_UUID)
            if (rx == null || tx == null) {
                pendingConnect?.completeExceptionally(IllegalStateException("HomeGuard BLE characteristics missing"))
                return
            }
            if (!gatt.setCharacteristicNotification(tx, true)) {
                pendingConnect?.completeExceptionally(IllegalStateException("Cannot enable BLE notifications"))
                return
            }
            val descriptor = tx?.getDescriptor(CCCD_UUID)
            if (descriptor == null) {
                pendingConnect?.completeExceptionally(IllegalStateException("BLE notification descriptor missing"))
                return
            }
            if (Build.VERSION.SDK_INT >= 33) {
                if (gatt.writeDescriptor(descriptor, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) != BluetoothGatt.GATT_SUCCESS) {
                    pendingConnect?.completeExceptionally(IllegalStateException("Cannot write BLE notification descriptor"))
                }
            } else {
                @Suppress("DEPRECATION")
                run {
                    descriptor.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                    if (!gatt.writeDescriptor(descriptor)) {
                        pendingConnect?.completeExceptionally(IllegalStateException("Cannot write BLE notification descriptor"))
                    }
                }
            }
        }

        override fun onDescriptorWrite(gatt: BluetoothGatt, descriptor: BluetoothGattDescriptor, status: Int) {
            if (descriptor.uuid != CCCD_UUID) return
            if (status == BluetoothGatt.GATT_SUCCESS) pendingConnect?.complete(Unit)
            else pendingConnect?.completeExceptionally(IllegalStateException("BLE notifications rejected: $status"))
        }

        override fun onCharacteristicWrite(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, status: Int) {
            if (characteristic.uuid != RX_UUID) return
            val deferred = pendingWrite
            pendingWrite = null
            if (status == BluetoothGatt.GATT_SUCCESS) deferred?.complete(Unit)
            else deferred?.completeExceptionally(IllegalStateException("BLE write failed: $status"))
        }

        @Deprecated("Deprecated in API 33")
        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic) {
            consumeFragment(characteristic.value ?: return)
        }

        override fun onCharacteristicChanged(gatt: BluetoothGatt, characteristic: BluetoothGattCharacteristic, value: ByteArray) {
            consumeFragment(value)
        }
    }

    fun isAvailable(): Boolean = manager.adapter?.isEnabled == true && hasPermissions()

    @SuppressLint("MissingPermission")
    suspend fun provision(qr: ProvisioningQrData, form: ProvisioningForm, localApiToken: String) {
        check(isAvailable()) { "Bluetooth недоступний або немає дозволу" }
        val device = scanForController(qr.deviceId)
        connect(device)
        val authorized = exchange(
            AUTHORIZE_TYPE,
            JSONObject()
                .put("pairing_code", qr.pairingCode)
                .put("certificate_sha256", qr.certificateSha256)
                .toString()
        )
        check(authorized.ok && authorized.stage == "authorized") {
            "BLE authorization failed: ${authorized.reason.ifBlank { authorized.stage }}"
        }
        val applied = exchange(
            APPLY_TYPE,
            JSONObject()
                .put("wifi_ssid", form.wifiSsid)
                .put("wifi_password", form.wifiPassword)
                .put("local_api_token", localApiToken)
                .put("owner_label", form.ownerLabel)
                .put("cloud_endpoint", form.cloudEndpoint)
                .put("cloud_token", form.cloudClaimToken)
                .toString()
        )
        check(applied.ok && applied.stage == "applied") {
            "BLE provisioning failed: ${applied.reason.ifBlank { applied.stage }}"
        }
    }

    @SuppressLint("MissingPermission")
    private suspend fun scanForController(deviceId: String): BluetoothDevice = withContext(Dispatchers.Main.immediate) {
        val scanner = manager.adapter.bluetoothLeScanner ?: error("BLE scanner unavailable")
        val expectedSuffix = deviceId.takeLast(6).uppercase()
        val result = CompletableDeferred<BluetoothDevice>()
        val callback = object : ScanCallback() {
            override fun onScanResult(callbackType: Int, scanResult: ScanResult) {
                val name = scanResult.scanRecord?.deviceName ?: scanResult.device.name.orEmpty()
                val normalized = name.replace("-", "").uppercase()
                if (normalized.endsWith(expectedSuffix)) result.complete(scanResult.device)
            }

            override fun onScanFailed(errorCode: Int) {
                result.completeExceptionally(IllegalStateException("BLE scan failed: $errorCode"))
            }
        }
        val filter = ScanFilter.Builder().setServiceUuid(ParcelUuid(SERVICE_UUID)).build()
        val settings = ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build()
        scanner.startScan(listOf(filter), settings, callback)
        try {
            withTimeout(12_000L) { result.await() }
        } finally {
            scanner.stopScan(callback)
        }
    }

    @SuppressLint("MissingPermission")
    private suspend fun connect(device: BluetoothDevice) = withContext(Dispatchers.Main.immediate) {
        close()
        val deferred = CompletableDeferred<Unit>()
        pendingConnect = deferred
        gatt = if (Build.VERSION.SDK_INT >= 26) {
            device.connectGatt(context, false, callback, BluetoothDevice.TRANSPORT_LE, BluetoothDevice.PHY_LE_1M_MASK)
        } else {
            @Suppress("DEPRECATION")
            device.connectGatt(context, false, callback)
        }
        try {
            withTimeout(12_000L) { deferred.await() }
        } finally {
            pendingConnect = null
        }
    }

    private suspend fun exchange(type: Byte, payload: String): Reply {
        send(type, payload)
        while (true) {
            val (replyType, json) = withTimeout(12_000L) { incoming.receive() }
            if (replyType != REPLY_TYPE) continue
            val body = JSONObject(json)
            return Reply(
                ok = body.optBoolean("ok", false),
                stage = body.optString("stage", ""),
                reason = body.optString("reason", "")
            )
        }
    }

    @SuppressLint("MissingPermission")
    private suspend fun send(type: Byte, payload: String) {
        val characteristic = rx ?: error("BLE RX characteristic unavailable")
        val bytes = payload.toByteArray(Charsets.UTF_8)
        val mtuPayload = 180
        val count = maxOf(1, (bytes.size + mtuPayload - 1) / mtuPayload)
        require(count <= 255) { "BLE payload too large" }
        val id = messageId.getAndIncrement() and 0xffff
        for (index in 0 until count) {
            val start = index * mtuPayload
            val end = minOf(bytes.size, start + mtuPayload)
            val frame = ByteArray(HEADER_SIZE + (end - start))
            frame[0] = PROTOCOL_VERSION
            frame[1] = type
            frame[2] = (id and 0xff).toByte()
            frame[3] = ((id ushr 8) and 0xff).toByte()
            frame[4] = index.toByte()
            frame[5] = count.toByte()
            if (end > start) bytes.copyInto(frame, HEADER_SIZE, start, end)
            val deferred = CompletableDeferred<Unit>()
            pendingWrite = deferred
            val started = if (Build.VERSION.SDK_INT >= 33) {
                gatt?.writeCharacteristic(characteristic, frame, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT) == BluetoothGatt.GATT_SUCCESS
            } else {
                @Suppress("DEPRECATION")
                run {
                    characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
                    characteristic.value = frame
                    gatt?.writeCharacteristic(characteristic) == true
                }
            }
            if (!started) {
                pendingWrite = null
                error("BLE write did not start")
            }
            withTimeout(6_000L) { deferred.await() }
        }
    }

    private fun consumeFragment(value: ByteArray) {
        if (value.size < HEADER_SIZE || value[0] != PROTOCOL_VERSION) return
        val type = value[1]
        val id = (value[2].toInt() and 0xff) or ((value[3].toInt() and 0xff) shl 8)
        val index = value[4].toInt() and 0xff
        val count = value[5].toInt() and 0xff
        if (count == 0 || index >= count) return
        if (index == 0) {
            rxType = type
            rxMessageId = id
            rxExpectedCount = count
            rxNextIndex = 0
            rxPayload.clear()
        }
        if (type != rxType || id != rxMessageId || count != rxExpectedCount || index != rxNextIndex) {
            rxPayload.clear()
            rxExpectedCount = 0
            return
        }
        for (i in HEADER_SIZE until value.size) rxPayload.add(value[i])
        rxNextIndex++
        if (rxNextIndex == rxExpectedCount) {
            val bytes = ByteArray(rxPayload.size) { rxPayload[it] }
            incoming.trySend(type to bytes.toString(Charsets.UTF_8))
            rxPayload.clear()
            rxExpectedCount = 0
        }
    }

    private fun hasPermissions(): Boolean {
        return if (Build.VERSION.SDK_INT >= 31) {
            ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_SCAN) == PackageManager.PERMISSION_GRANTED &&
                ContextCompat.checkSelfPermission(context, Manifest.permission.BLUETOOTH_CONNECT) == PackageManager.PERMISSION_GRANTED
        } else {
            ContextCompat.checkSelfPermission(context, Manifest.permission.ACCESS_FINE_LOCATION) == PackageManager.PERMISSION_GRANTED
        }
    }

    @SuppressLint("MissingPermission")
    override fun close() {
        pendingConnect?.cancel()
        pendingConnect = null
        pendingWrite?.cancel()
        pendingWrite = null
        gatt?.disconnect()
        gatt?.close()
        gatt = null
        rx = null
        tx = null
    }
}
