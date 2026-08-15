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
        val endpoint = normalizeEndpoint(device.baseUrl)
        val exactIndex = current.indexOfFirst { it.deviceId == device.deviceId }
        val endpointIndex = if (endpoint.isNotBlank()) current.indexOfFirst { normalizeEndpoint(it.baseUrl) == endpoint } else -1
        val previousIndex = if (exactIndex >= 0) exactIndex else endpointIndex
        val previous = current.getOrNull(previousIndex)
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

        // If the same controller was saved earlier with a temporary/stale device ID,
        // replace that record instead of creating a second HomeGuard card.
        if (previousIndex >= 0) current[previousIndex] = registered else current += registered
        current.removeAll { it !== registered && it.deviceId != registered.deviceId && endpoint.isNotBlank() && normalizeEndpoint(it.baseUrl) == endpoint }
        persist(current)
    }

    suspend fun addManual(deviceId: String, baseUrl: String, name: String = "HomeGuard") {
        val current = _devices.value.toMutableList()
        val endpoint = normalizeEndpoint(baseUrl)
        val idIndex = current.indexOfFirst { it.deviceId == deviceId }
        val endpointIndex = if (endpoint.isNotBlank()) current.indexOfFirst { normalizeEndpoint(it.baseUrl) == endpoint } else -1
        val index = if (idIndex >= 0) idIndex else endpointIndex
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
        val normalizedDiscoveredUrl = normalizeEndpoint(discovered.baseUrl)
        val current = _devices.value.toMutableList()
        val manualIndex = current.indexOfFirst {
            it.deviceId == manualDeviceId && normalizeEndpoint(it.baseUrl) == normalizedDiscoveredUrl
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
            if (manualIndex != realIndex) current.removeAt(manualIndex)
        } else {
            current[manualIndex] = merged
        }
        persist(current)
        return true
    }

    suspend fun refreshDiscovered(discovered: DiscoveredDevice): Boolean {
        val current = _devices.value.toMutableList()
        val endpoint = normalizeEndpoint(discovered.baseUrl)
        val idIndex = current.indexOfFirst { it.deviceId == discovered.deviceId }
        val endpointIndex = if (endpoint.isNotBlank()) current.indexOfFirst { normalizeEndpoint(it.baseUrl) == endpoint } else -1
        val index = if (idIndex >= 0) idIndex else endpointIndex
        if (index < 0) return false

        val previous = current[index]
        val updated = previous.copy(
            deviceId = discovered.deviceId,
            baseUrl = discovered.baseUrl,
            lastSeenAtMs = discovered.seenAtMs,
        )
        if (previous == updated) return false
        current[index] = updated
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
        val clean = deduplicate(value)
        val json = JSONArray()
        clean.forEach { device ->
            json.put(JSONObject().apply {
                put("device_id", device.deviceId)
                put("name", device.name)
                put("base_url", device.baseUrl)
                put("last_seen_at_ms", device.lastSeenAtMs)
                put("authorized", device.authorized)
            })
        }
        preferences.edit().putString("devices", json.toString()).apply()
        _devices.emit(clean.sortedBy { it.name.lowercase() })
    }

    private fun load(): List<RegisteredDevice> = runCatching {
        val raw = preferences.getString("devices", "[]").orEmpty()
        val array = JSONArray(raw)
        val loaded = buildList {
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
        }
        deduplicate(loaded).sortedBy { it.name.lowercase() }
    }.getOrDefault(emptyList())

    private fun deduplicate(value: List<RegisteredDevice>): List<RegisteredDevice> {
        val result = mutableListOf<RegisteredDevice>()
        value.sortedByDescending { it.lastSeenAtMs }.forEach { candidate ->
            val endpoint = normalizeEndpoint(candidate.baseUrl)
            val duplicate = result.any { existing ->
                existing.deviceId == candidate.deviceId ||
                    (endpoint.isNotBlank() && normalizeEndpoint(existing.baseUrl) == endpoint)
            }
            if (!duplicate) result += candidate
        }
        return result
    }

    private fun normalizeEndpoint(value: String): String = value.trim().trimEnd('/').lowercase()
}
