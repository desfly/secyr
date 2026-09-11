package ua.homeguard.s3.network.ble

import android.content.Context

/**
 * Provides one authenticated BLE runtime per app process so command and telemetry
 * paths share the same BLE connection/session instead of opening competing GATT links.
 */
object BleRuntimeRegistry {
    @Volatile
    private var runtime: BleRuntimeSession? = null

    fun get(context: Context): BleRuntimeSession {
        runtime?.let { return it }
        return synchronized(this) {
            runtime ?: BleRuntimeSession(context.applicationContext).also { runtime = it }
        }
    }
}
