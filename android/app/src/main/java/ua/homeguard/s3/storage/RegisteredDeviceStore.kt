package ua.homeguard.s3.storage

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.json.JSONArray
import org.json.JSONObject
import ua.homeguard.s3.model.DiscoveredDevice

data class RegisteredDevice(
    val deviceId: String,
    val name: String,
    val baseUrl: String,
    val lastSeenAtMs: Long,
    val authorized: Boolean = true,
)

class RegisteredDeviceStore(context: Context) {
    companion object {
        @Volatile private var activeStore: RegisteredDeviceStore? = null

        suspend fun markActiveAuthorization(deviceId: String, authorized: Boolean) {
            if (deviceId.isBlank()) return
            activeStore?.markAuthorization(deviceId, authorized)
        }

        suspend fun reconcileActiveManual(manualDeviceId: String, discovered: DiscoveredDevice): Boolean {
            return activeStore?.reconcileManual(manualDeviceId, discovered) ?: false
        }

        suspend fun refreshActiveDiscovered(discovered: DiscoveredDevice): Boolean {
            return activeStore?.refreshDiscovered(discovered) ?: false
        }

        suspend fun removeActive(deviceId: String): Boolean {
            if (deviceId.isBlank()) return false
            return activeStore?.remove(deviceId) ?: false
        }
    }

    private val preferences = context.applicationContext.getSharedPreferences("homeguard_devices", Context.MODE_PRIVATE)
    private val _devices = MutableStateFlow(load())
    val devices: StateFlow<List<RegisteredDevice>> = _devices.asStateFlow()

    init { activeStore = this }

    suspend fun addOrUpdate(device: DiscoveredDevice, requestedName: String? = null) {
        val current = _devices.value.toMutableList()
        val index = current.indexOfFirst { it.deviceId == device.deviceId }
        val previous = current.getOrNull(index)
        val displayName = requestedName?.trim().takeUnless { it.isNullOrBlank() }
            ?: previous?.name
            ?: device.serviceName.takeIf { it.isNotBlank() }
            ?: "HomeGuard"
        val registered = RegisteredDevice(
            deviceId = device.deviceId,
            name = displayName,
            baseUrl = device.baseUrl,
            lastSeenAtMs = device.seenAtMs,
            authorized = previous?.authorized ?: true,
        )
        if (index >= 0) current[index] = registered else current += registered
        persist(current)
    }

    suspend fun addManual(deviceId: String, baseUrl: String, name: String = "HomeGuard") {
        val current = _devices.value.toMutableList()
        val index = current.indexOfFirst { it.deviceId == deviceId }
        val previous = current.getOrNull(index)
        val registered = RegisteredDevice(
            deviceId = deviceId,
            name = previous?.name ?: name,
            baseUrl = baseUrl,
            lastSeenAtMs = System.currentTimeMillis(),
            authorized = previous?.authorized ?: true,
        )
        if (index >= 0) current[index] = registered else current += registered
        persist(current)
    }

    suspend fun reconcileManual(manualDeviceId: String, discovered: DiscoveredDevice): Boolean {
        if (!manualDeviceId.startsWith("manual-") || discovered.deviceId.isBlank()) return false
        val normalizedDiscoveredUrl = discovered.baseUrl.trimEnd('/')
        val current = _devices.value.toMutableList()
        val manualIndex = current.indexOfFirst {
            it.deviceId == manualDeviceId && it.baseUrl.trimEnd('/') == normalizedDiscoveredUrl
        }
        if (manualIndex < 0) return false

        val manual = current[manualIndex]
        val realIndex = current.indexOfFirst { it.deviceId == discovered.deviceId }
        val merged = RegisteredDevice(
            deviceId = discovered.deviceId,
            name = manual.name,
            baseUrl = discovered.baseUrl,
            lastSeenAtMs = discovered.seenAtMs,
            authorized = if (realIndex >= 0) current[realIndex].authorized else manual.authorized,
        )

        if (realIndex >= 0) {
            current[realIndex] = merged
            current.removeAt(manualIndex)
        } else {
            current[manualIndex] = merged
        }
        persist(current.distinctBy { it.deviceId })
        return true
    }

    suspend fun refreshDiscovered(discovered: DiscoveredDevice): Boolean {
        val current = _devices.value.toMutableList()
        val index = current.indexOfFirst { it.deviceId == discovered.deviceId }
        if (index < 0) return false

        val previous = current[index]
        val normalizedOld = previous.baseUrl.trimEnd('/')
        val normalizedNew = discovered.baseUrl.trimEnd('/')
        if (normalizedOld == normalizedNew && previous.lastSeenAtMs == discovered.seenAtMs) return false

        current[index] = previous.copy(
            baseUrl = discovered.baseUrl,
            lastSeenAtMs = discovered.seenAtMs,
        )
        persist(current)
        return true
    }

    suspend fun rename(deviceId: String, name: String) {
        val clean = name.trim().take(40)
        if (clean.isBlank()) return
        persist(_devices.value.map { if (it.deviceId == deviceId) it.copy(name = clean) else it })
    }

    suspend fun remove(deviceId: String): Boolean {
        val updated = _devices.value.filterNot { it.deviceId == deviceId }
        if (updated.size == _devices.value.size) return false
        persist(updated)
        return true
    }

    suspend fun markAuthorization(deviceId: String, authorized: Boolean) {
        if (_devices.value.none { it.deviceId == deviceId }) return
        persist(_devices.value.map { if (it.deviceId == deviceId) it.copy(authorized = authorized) else it })
    }

    private suspend fun persist(value: List<RegisteredDevice>) {
        val json = JSONArray()
        value.forEach { device ->
            json.put(JSONObject().apply {
                put("device_id", device.deviceId)
                put("name", device.name)
                put("base_url", device.baseUrl)
                put("last_seen_at_ms", device.lastSeenAtMs)
                put("authorized", device.authorized)
            })
        }
        preferences.edit().putString("devices", json.toString()).apply()
        _devices.emit(value.sortedBy { it.name.lowercase() })
    }

    private fun load(): List<RegisteredDevice> = runCatching {
        val raw = preferences.getString("devices", "[]").orEmpty()
        val array = JSONArray(raw)
        buildList {
            for (index in 0 until array.length()) {
                val item = array.getJSONObject(index)
                val id = item.optString("device_id")
                if (id.isBlank()) continue
                add(
                    RegisteredDevice(
                        deviceId = id,
                        name = item.optString("name", "HomeGuard"),
                        baseUrl = item.optString("base_url"),
                        lastSeenAtMs = item.optLong("last_seen_at_ms"),
                        authorized = item.optBoolean("authorized", true),
                    )
                )
            }
        }.sortedBy { it.name.lowercase() }
    }.getOrDefault(emptyList())
}
