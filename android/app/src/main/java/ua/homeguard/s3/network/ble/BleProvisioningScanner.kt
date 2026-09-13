package ua.homeguard.s3.network.ble

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.util.Log
import kotlinx.coroutines.TimeoutCancellationException
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withTimeout
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

class BleProvisioningScanner(private val context: Context) {
    companion object {
        private const val TAG = "HomeGuardBLE"
    }

    @SuppressLint("MissingPermission")
    suspend fun find(deviceId: String, timeoutMs: Long = 12_000L): BluetoothDevice {
        requirePermissions()
        val manager = context.getSystemService(BluetoothManager::class.java)
            ?: error("Bluetooth недоступний")
        val adapter = manager.adapter ?: error("Bluetooth адаптер відсутній")
        require(adapter.isEnabled) { "Увімкніть Bluetooth" }
        val scanner = adapter.bluetoothLeScanner ?: error("BLE сканер недоступний")
        val expectedSuffix = deviceId.takeLast(6).uppercase()

        // Four seconds was too aggressive for the opportunistic post-HTTP-login
        // BLE path on real phones. Keep BLE independent, but always give the
        // controller a full low-latency scan window before declaring it absent.
        val effectiveTimeoutMs = timeoutMs.coerceAtLeast(12_000L)

        return try {
            withTimeout(effectiveTimeoutMs) {
                suspendCancellableCoroutine { continuation ->
                    val callback = object : ScanCallback() {
                        override fun onScanResult(callbackType: Int, result: ScanResult) {
                            val record = result.scanRecord
                            val advertisedName = record?.deviceName.orEmpty().uppercase()
                            val cachedName = runCatching { result.device.name.orEmpty().uppercase() }.getOrDefault("")
                            val nameMatches = sequenceOf(advertisedName, cachedName)
                                .filter { it.isNotBlank() }
                                .any { name ->
                                    name.startsWith("HOMEGUARD-S3") &&
                                        (name.endsWith(expectedSuffix) || name == "HOMEGUARD-S3")
                                }
                            val serviceMatches = record?.serviceUuids
                                ?.any { it.uuid == HomeGuardBleContract.SERVICE_UUID } == true

                            // Do not depend on Android controller-offloaded UUID scan
                            // filters. Some phones fail to surface the ESP32-S3 result
                            // through a filtered scan even though the same ScanRecord
                            // contains the HomeGuard UUID. An unfiltered app-side match
                            // accepts only our service UUID or our canonical device name.
                            if (!serviceMatches && !nameMatches) return

                            Log.i(TAG, "BLE scan matched ${result.device.address}; service=$serviceMatches name=$advertisedName")
                            runCatching { scanner.stopScan(this) }
                            if (continuation.isActive) continuation.resume(result.device)
                        }

                        override fun onScanFailed(errorCode: Int) {
                            if (continuation.isActive) {
                                continuation.resumeWithException(IllegalStateException("BLE scan failed: $errorCode"))
                            }
                        }
                    }

                    continuation.invokeOnCancellation { runCatching { scanner.stopScan(callback) } }
                    val settings = ScanSettings.Builder()
                        .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                        .setReportDelay(0L)
                        .build()
                    scanner.startScan(null, settings, callback)
                }
            }
        } catch (timeout: TimeoutCancellationException) {
            // HomeGuard device_id is derived from ESP_MAC_WIFI_STA. This firmware
            // uses the ESP32-S3 default four-universal-MAC layout, where Bluetooth
            // is base/Wi-Fi STA + 2. A direct GATT attempt bypasses vendor Android
            // scan filtering while preserving the normal advertised UUID/name path.
            val address = deriveEsp32S3BluetoothAddress(deviceId)
                ?: throw IllegalStateException("BLE scan timeout and device id has no derivable ESP32-S3 address", timeout)
            Log.w(TAG, "BLE scan timeout; trying deterministic controller address $address")
            runCatching { adapter.getRemoteDevice(address) }
                .getOrElse { throw IllegalStateException("BLE scan timeout; invalid derived address $address", it) }
        }
    }

    private fun deriveEsp32S3BluetoothAddress(deviceId: String): String? {
        val raw = deviceId.trim().uppercase().removePrefix("HG-")
        if (raw.length != 12 || raw.any { it !in '0'..'9' && it !in 'A'..'F' }) return null

        val bytes = IntArray(6) { index ->
            raw.substring(index * 2, index * 2 + 2).toInt(16)
        }
        var carry = 2
        for (index in bytes.lastIndex downTo 0) {
            if (carry == 0) break
            val sum = bytes[index] + carry
            bytes[index] = sum and 0xff
            carry = sum ushr 8
        }
        return bytes.joinToString(":") { value -> "%02X".format(value) }
    }

    private fun requirePermissions() {
        val required = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }
        require(required.all { context.checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }) {
            "Надайте застосунку дозвіл Bluetooth/пристрої поблизу"
        }
    }
}
