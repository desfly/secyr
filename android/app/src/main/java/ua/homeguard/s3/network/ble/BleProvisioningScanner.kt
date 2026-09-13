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
        BleRuntimeDiagnostics.update("SCAN_PRECHECK", "checking permissions and adapter")
        requirePermissions()
        val manager = context.getSystemService(BluetoothManager::class.java)
            ?: run {
                BleRuntimeDiagnostics.update("SCAN_ERROR", "Bluetooth manager unavailable")
                error("Bluetooth недоступний")
            }
        val adapter = manager.adapter ?: run {
            BleRuntimeDiagnostics.update("SCAN_ERROR", "Bluetooth adapter missing")
            error("Bluetooth адаптер відсутній")
        }
        if (!adapter.isEnabled) {
            BleRuntimeDiagnostics.update("SCAN_ERROR", "Bluetooth disabled")
            error("Увімкніть Bluetooth")
        }
        val scanner = adapter.bluetoothLeScanner ?: run {
            BleRuntimeDiagnostics.update("SCAN_ERROR", "BLE scanner unavailable")
            error("BLE сканер недоступний")
        }
        val expectedSuffix = deviceId.takeLast(6).uppercase()

        val effectiveTimeoutMs = timeoutMs.coerceAtLeast(12_000L)
        BleRuntimeDiagnostics.update("SCANNING", "low-latency ${effectiveTimeoutMs}ms")

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

                            if (!serviceMatches && !nameMatches) return

                            val address = result.device.address
                            Log.i(TAG, "BLE scan matched $address; service=$serviceMatches name=$advertisedName")
                            BleRuntimeDiagnostics.update(
                                stage = "SCAN_MATCHED",
                                detail = "service=$serviceMatches name=${advertisedName.ifBlank { cachedName.ifBlank { "<none>" } }}",
                                address = address,
                            )
                            runCatching { scanner.stopScan(this) }
                            if (continuation.isActive) continuation.resume(result.device)
                        }

                        override fun onScanFailed(errorCode: Int) {
                            BleRuntimeDiagnostics.update("SCAN_FAILED", "Android scan callback failed", errorCode)
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
            BleRuntimeDiagnostics.update(
                stage = "SCAN_TIMEOUT",
                detail = "no HomeGuard advertisement seen in ${effectiveTimeoutMs}ms",
            )
            throw IllegalStateException("BLE scan timeout: HomeGuard advertisement not seen", timeout)
        }
    }

    private fun requirePermissions() {
        val required = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT)
        } else {
            arrayOf(Manifest.permission.ACCESS_FINE_LOCATION)
        }
        val missing = required.filter { context.checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }
        if (missing.isNotEmpty()) {
            BleRuntimeDiagnostics.update("SCAN_PERMISSION", "missing: ${missing.joinToString()}")
        }
        require(missing.isEmpty()) {
            "Надайте застосунку дозвіл Bluetooth/пристрої поблизу"
        }
    }
}
