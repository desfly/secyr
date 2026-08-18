package ua.homeguard.s3.storage

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.json.JSONArray
import org.json.JSONObject
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.network.ControllerIdentity

data class RegisteredDevice(
    val deviceId: String,
    val name: String,
    val baseUrl: String,
    val lastSeenAtMs: Long,
    val authorized: Boolean = true,
)

class RegisteredDeviceStore(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences("homeguard_devices", Context.MODE_PRIVATE)
    private val _devices = MutableStateFlow(load())
    val devices: StateFlow<List<RegisteredDevice>> = _devices.asStateFlow()

    suspend fun addOrUpdate(device: DiscoveredDevice, requestedName: String? = null) {
        val current = _devices.value.toMutableList()
        val previousIndex = current.indexOfFirst { candidate ->
            ControllerIdentity.sameController(candidate.deviceId, candidate.baseUrl, device.deviceId, device.baseUrl)
        }
        val previous = current.getOrNull(previousIndex)
        val requested = requestedName?.trim()?.take(40).orEmpty()
        if (previous == null && requested.isBlank()) return

        val registered = RegisteredDevice(
            deviceId = device.deviceId.trim(),
            name = requested.takeIf { it.isNotBlank() } ?: previous!!.name,
            baseUrl = device.baseUrl,
            lastSeenAtMs = device.seenAtMs,
            authorized = previous?.authorized ?: true,
        )

        if (previousIndex >= 0) current[previousIndex] = registered else current += registered
        val keepIndex = if (previousIndex >= 0) previousIndex else current.lastIndex
        current.indices.reversed().forEach { index ->
            if (index != keepIndex) {
                val candidate = current[index]
                if (ControllerIdentity.sameController(candidate.deviceId, candidate.baseUrl, registered.deviceId, registered.baseUrl)) {
                    current.removeAt(index)
                }
            }
        }
        persist(current)
    }

    suspend fun addManual(deviceId: String, baseUrl: String, name: String) {
        val cleanName = name.trim().take(40)
        if (deviceId.isBlank() || cleanName.isBlank()) return

        val current = _devices.value.toMutableList()
        val index = current.indexOfFirst { candidate ->
            ControllerIdentity.sameController(candidate.deviceId, candidate.baseUrl, deviceId, baseUrl)
        }
        val previous = current.getOrNull(index)
        val registered = RegisteredDevice(
            deviceId = deviceId.trim(),
            name = cleanName,
            baseUrl = baseUrl,
            lastSeenAtMs = System.currentTimeMillis(),
            authorized = previous?.authorized ?: true,
        )
        if (index >= 0) current[index] = registered else current += registered
        persist(current)
    }

    suspend fun reconcileManual(manualDeviceId: String, discovered: DiscoveredDevice): Boolean {
        if (!manualDeviceId.startsWith("manual-", ignoreCase = true) || discovered.deviceId.isBlank()) return false
        val current = _devices.value.toMutableList()
        val manualIndex = current.indexOfFirst { candidate ->
            candidate.deviceId.equals(manualDeviceId, ignoreCase = true) &&
                ControllerIdentity.sameController(candidate.deviceId, candidate.baseUrl, discovered.deviceId, discovered.baseUrl)
        }
        if (manualIndex < 0) return false

        val manual = current[manualIndex]
        val realIndex = current.indexOfFirst { candidate ->
            ControllerIdentity.sameController(candidate.deviceId, candidate.baseUrl, discovered.deviceId, discovered.baseUrl)
        }
        val merged = RegisteredDevice(
            deviceId = discovered.deviceId.trim(),
            name = manual.name,
            baseUrl = discovered.baseUrl,
            lastSeenAtMs = discovered.seenAtMs,
            authorized = if (realIndex >= 0) current[realIndex].authorized else manual.authorized,
        )

        if (realIndex >= 0 && realIndex != manualIndex) {
            current[realIndex] = merged
            current.removeAt(manualIndex)
        } else {
            current[manualIndex] = merged
        }
        persist(current)
        return true
    }

    suspend fun refreshDiscovered(discovered: DiscoveredDevice): Boolean {
        val current = _devices.value.toMutableList()
        val index = current.indexOfFirst { candidate ->
            ControllerIdentity.sameController(candidate.deviceId, candidate.baseUrl, discovered.deviceId, discovered.baseUrl)
        }
        if (index < 0) return false

        val previous = current[index]
        val updated = previous.copy(
            deviceId = discovered.deviceId.trim(),
            baseUrl = discovered.baseUrl,
            lastSeenAtMs = discovered.seenAtMs,
        )
        current[index] = updated
        val beforeCount = current.size
        persist(current)
        return previous != updated || beforeCount != _devices.value.size
    }

    suspend fun rename(deviceId: String, name: String) {
        val clean = name.trim().take(40)
        if (clean.isBlank()) return
        persist(_devices.value.map {
            if (it.deviceId.equals(deviceId.trim(), ignoreCase = true)) it.copy(name = clean) else it
        })
    }

    suspend fun remove(deviceId: String): Boolean {
        val cleanId = deviceId.trim()
        val updated = _devices.value.filterNot { it.deviceId.equals(cleanId, ignoreCase = true) }
        if (updated.size == _devices.value.size) return false
        persist(updated)
        return true
    }

    suspend fun markAuthorization(deviceId: String, authorized: Boolean) {
        val cleanId = deviceId.trim()
        if (_devices.value.none { it.deviceId.equals(cleanId, ignoreCase = true) }) return
        persist(_devices.value.map {
            if (it.deviceId.equals(cleanId, ignoreCase = true)) it.copy(authorized = authorized) else it
        })
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
                        deviceId = id.trim(),
                        name = item.optString("name").trim().take(40),
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
            val duplicate = result.any { existing ->
                ControllerIdentity.sameController(
                    existing.deviceId,
                    existing.baseUrl,
                    candidate.deviceId,
                    candidate.baseUrl,
                )
            }
            if (!duplicate) result += candidate
        }
        return result
    }
}
