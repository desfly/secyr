package ua.homeguard.s3.network.ble

import android.Manifest
import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothManager
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.ParcelUuid
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlinx.coroutines.withTimeout
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

class BleProvisioningScanner(private val context: Context) {
    @SuppressLint("MissingPermission")
    suspend fun find(deviceId: String, timeoutMs: Long = 12_000L): BluetoothDevice = withTimeout(timeoutMs) {
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
                    val advertisedName = result.scanRecord?.deviceName.orEmpty().uppercase()
                    if (!advertisedName.startsWith("HOMEGUARD-S3-") ||
                        !advertisedName.endsWith(expectedSuffix)) return
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
            val filter = ScanFilter.Builder()
                .setServiceUuid(ParcelUuid(HomeGuardBleContract.SERVICE_UUID))
                .build()
            val settings = ScanSettings.Builder()
                .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
                .build()
            scanner.startScan(listOf(filter), settings, callback)
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
