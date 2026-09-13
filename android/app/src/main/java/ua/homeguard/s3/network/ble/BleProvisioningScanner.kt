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
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withTimeout
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

class BleProvisioningScanner(private val context: Context) {
    @SuppressLint("MissingPermission")
    suspend fun find(deviceId: String, timeoutMs: Long = 12_000L): BluetoothDevice {
        // Four seconds was too aggressive for the opportunistic post-HTTP-login
        // BLE path on real phones. Keep BLE independent, but always give the
        // controller a full low-latency scan window before declaring it absent.
        val effectiveTimeoutMs = timeoutMs.coerceAtLeast(12_000L)
        return withTimeout(effectiveTimeoutMs) {
            requirePermissions()
            val manager = context.getSystemService(BluetoothManager::class.java)
                ?: error("Bluetooth недоступний")
            val adapter = manager.adapter ?: error("Bluetooth адаптер відсутній")
            require(adapter.isEnabled) { "Увімкніть Bluetooth" }
            val scanner = adapter.bluetoothLeScanner ?: error("BLE сканер недоступний")
            val expectedSuffix = deviceId.takeLast(6).uppercase()

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
                    .build()
                scanner.startScan(null, settings, callback)
            }
        }
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
