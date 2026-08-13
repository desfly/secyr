package ua.homeguard.s3.storage

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import org.json.JSONArray
import org.json.JSONObject
import ua.homeguard.s3.model.RegisteredDevice
import ua.homeguard.s3.model.RegisteredDeviceAccess

class RegisteredDeviceStore(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences("homeguard_registered_devices", Context.MODE_PRIVATE)
    private val mutable = MutableStateFlow(load())
    val devices: StateFlow<List<RegisteredDevice>> = mutable

    fun upsert(device: RegisteredDevice) {
        val updated = mutable.value.toMutableList()
        val index = updated.indexOfFirst { it.deviceId == device.deviceId }
        if (index >= 0) updated[index] = device else updated += device
        persist(updated)
    }

    fun rename(deviceId: String, newName: String) {
        persist(mutable.value.map { if (it.deviceId == deviceId) it.renamed(newName) else it })
    }

    fun updateAccess(
        deviceId: String,
        access: RegisteredDeviceAccess,
        lastSeenAtMs: Long? = null,
        verifiedAtMs: Long = System.currentTimeMillis(),
    ) {
        persist(mutable.value.map { device ->
            if (device.deviceId != deviceId) device
            else device.copy(
                access = access,
                lastSeenAtMs = lastSeenAtMs ?: device.lastSeenAtMs,
                lastVerifiedAtMs = verifiedAtMs,
            )
        })
    }

    fun remove(deviceId: String) {
        persist(mutable.value.filterNot { it.deviceId == deviceId })
    }

    private fun persist(value: List<RegisteredDevice>) {
        val array = JSONArray()
        value.forEach { device ->
            array.put(
                JSONObject()
                    .put("deviceId", device.deviceId)
                    .put("displayName", device.displayName)
                    .put("lastKnownUrl", device.lastKnownUrl)
                    .put("certificateSha256", device.certificateSha256)
                    .put("access", device.access.name)
                    .put("lastSeenAtMs", device.lastSeenAtMs)
                    .put("lastVerifiedAtMs", device.lastVerifiedAtMs)
            )
        }
        preferences.edit().putString(KEY_DEVICES, array.toString()).apply()
        mutable.value = value
    }

    private fun load(): List<RegisteredDevice> = runCatching {
        val raw = preferences.getString(KEY_DEVICES, "[]").orEmpty()
        val array = JSONArray(raw)
        buildList {
            for (index in 0 until array.length()) {
                val item = array.getJSONObject(index)
                add(
                    RegisteredDevice(
                        deviceId = item.getString("deviceId"),
                        displayName = item.optString("displayName", "HomeGuard"),
                        lastKnownUrl = item.optString("lastKnownUrl", ""),
                        certificateSha256 = item.optString("certificateSha256", ""),
                        access = runCatching {
                            RegisteredDeviceAccess.valueOf(item.optString("access", RegisteredDeviceAccess.UNKNOWN.name))
                        }.getOrDefault(RegisteredDeviceAccess.UNKNOWN),
                        lastSeenAtMs = item.optLong("lastSeenAtMs", 0L),
                        lastVerifiedAtMs = item.optLong("lastVerifiedAtMs", 0L),
                    )
                )
            }
        }
    }.getOrDefault(emptyList())

    private companion object {
        const val KEY_DEVICES = "devices_json_v1"
    }
}
