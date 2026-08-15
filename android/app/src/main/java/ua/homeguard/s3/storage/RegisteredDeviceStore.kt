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
        val matching = current.filter {
            it.deviceId.equals(device.deviceId, ignoreCase = true) ||
                (endpoint.isNotBlank() && normalizeEndpoint(it.baseUrl) == endpoint)
        }
        val exact = matching.firstOrNull { it.deviceId.equals(device.deviceId, ignoreCase = true) }
        val named = matching.firstOrNull { !isGeneratedName(it.name) }
        val previous = named ?: exact ?: matching.maxByOrNull { it.lastSeenAtMs }
        val requested = requestedName?.trim()?.take(40).orEmpty()
        val displayName = requested.ifBlank { previous?.name.orEmpty() }

        // A new controller is never persisted under a generated service name, device ID,
        // or IP address. The owner must assign a visible name first. Existing records keep
        // their owner-assigned name during discovery refresh/reconciliation.
        if (displayName.isBlank() || isGeneratedName(displayName)) return

        val registered = RegisteredDevice(
            deviceId = device.deviceId,
            name = displayName,
            baseUrl = device.baseUrl,
            lastSeenAtMs = device.seenAtMs,
            authorized = exact?.authorized ?: previous?.authorized ?: true,
        )

        // A physical controller may have been saved by ID, IP and discovery. Once the
        // real ID/endpoint is known, collapse every matching alias into one card.
        current.removeAll {
            it.deviceId.equals(device.deviceId, ignoreCase = true) ||
                (endpoint.isNotBlank() && normalizeEndpoint(it.baseUrl) == endpoint)
        }
        current += registered
        persist(current)
    }

    suspend fun addManual(deviceId: String, baseUrl: String, name: String = "") {
        val current = _devices.value.toMutableList()
        val endpoint = normalizeEndpoint(baseUrl)
        val idIndex = current.indexOfFirst { it.deviceId.equals(deviceId, ignoreCase = true) }
        val endpointIndex = if (endpoint.isNotBlank()) current.indexOfFirst { normalizeEndpoint(it.baseUrl) == endpoint } else -1
        val index = if (idIndex >= 0) idIndex else endpointIndex
        val previous = current.getOrNull(index)
        val requested = name.trim().take(40)
        val displayName = when {
            requested.isNotBlank() && !isGeneratedName(requested) -> requested
            previous != null && !isGeneratedName(previous.name) -> previous.name
            else -> ""
        }
        if (displayName.isBlank()) return

        val registered = RegisteredDevice(
            deviceId = deviceId,
            name = displayName,
            baseUrl = baseUrl,
            lastSeenAtMs = System.currentTimeMillis(),
            authorized = previous?.authorized ?: true,
        )
        if (index >= 0) current[index] = registered else current += registered
        persist(current)
    }

    suspend fun reconcileManual(manualDeviceId: String, discovered: DiscoveredDevice): Boolean {
        if (!manualDeviceId.startsWith("manual-") || discovered.deviceId.isBlank()) return false
        val endpoint = normalizeEndpoint(discovered.baseUrl)
        val current = _devices.value.toMutableList()
        val manual = current.firstOrNull {
            it.deviceId == manualDeviceId && normalizeEndpoint(it.baseUrl) == endpoint
        } ?: return false
        val real = current.firstOrNull { it.deviceId.equals(discovered.deviceId, ignoreCase = true) }
        val ownerName = when {
            !isGeneratedName(manual.name) -> manual.name
            real != null && !isGeneratedName(real.name) -> real.name
            else -> ""
        }
        if (ownerName.isBlank()) return false

        val merged = RegisteredDevice(
            deviceId = discovered.deviceId,
            name = ownerName,
            baseUrl = discovered.baseUrl,
            lastSeenAtMs = discovered.seenAtMs,
            authorized = real?.authorized ?: manual.authorized,
        )

        current.removeAll {
            it.deviceId == manualDeviceId ||
                it.deviceId.equals(discovered.deviceId, ignoreCase = true) ||
                (endpoint.isNotBlank() && normalizeEndpoint(it.baseUrl) == endpoint)
        }
        current += merged
        persist(current)
        return true
    }

    suspend fun refreshDiscovered(discovered: DiscoveredDevice): Boolean {
        val current = _devices.value.toMutableList()
        val endpoint = normalizeEndpoint(discovered.baseUrl)
        val matching = current.filter {
            it.deviceId.equals(discovered.deviceId, ignoreCase = true) ||
                (endpoint.isNotBlank() && normalizeEndpoint(it.baseUrl) == endpoint)
        }
        if (matching.isEmpty()) return false

        val exact = matching.firstOrNull { it.deviceId.equals(discovered.deviceId, ignoreCase = true) }
        val named = matching.firstOrNull { !isGeneratedName(it.name) }
        val previous = named ?: exact ?: matching.maxByOrNull { it.lastSeenAtMs } ?: return false
        if (isGeneratedName(previous.name)) return false

        val updated = RegisteredDevice(
            deviceId = discovered.deviceId,
            name = previous.name,
            baseUrl = discovered.baseUrl,
            lastSeenAtMs = discovered.seenAtMs,
            authorized = exact?.authorized ?: previous.authorized,
        )

        val alreadyCanonical = matching.size == 1 && previous == updated
        if (alreadyCanonical) return false

        current.removeAll {
            it.deviceId.equals(discovered.deviceId, ignoreCase = true) ||
                (endpoint.isNotBlank() && normalizeEndpoint(it.baseUrl) == endpoint)
        }
        current += updated
        persist(current)
        return true
    }

    suspend fun rename(deviceId: String, name: String) {
        val clean = name.trim().take(40)
        if (clean.isBlank() || isGeneratedName(clean)) return
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
        _devices.emit(clean.sortedBy { visibleSortName(it.name) })
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
                        name = item.optString("name", ""),
                        baseUrl = item.optString("base_url"),
                        lastSeenAtMs = item.optLong("last_seen_at_ms"),
                        authorized = item.optBoolean("authorized", true),
                    )
                )
            }
        }
        deduplicate(loaded).sortedBy { visibleSortName(it.name) }
    }.getOrDefault(emptyList())

    private fun deduplicate(value: List<RegisteredDevice>): List<RegisteredDevice> {
        val result = mutableListOf<RegisteredDevice>()
        value.sortedByDescending { it.lastSeenAtMs }.forEach { candidate ->
            val endpoint = normalizeEndpoint(candidate.baseUrl)
            val duplicateIndex = result.indexOfFirst { existing ->
                existing.deviceId.equals(candidate.deviceId, ignoreCase = true) ||
                    (endpoint.isNotBlank() && normalizeEndpoint(existing.baseUrl) == endpoint)
            }
            if (duplicateIndex < 0) {
                result += candidate
                return@forEach
            }

            val existing = result[duplicateIndex]
            val ownerName = when {
                !isGeneratedName(existing.name) -> existing.name
                !isGeneratedName(candidate.name) -> candidate.name
                else -> ""
            }
            val latest = if (existing.lastSeenAtMs >= candidate.lastSeenAtMs) existing else candidate
            result[duplicateIndex] = latest.copy(name = ownerName)
        }
        return result
    }

    private fun isGeneratedName(value: String): Boolean {
        val clean = value.trim()
        return clean.isBlank() ||
            clean.equals("HomeGuard", ignoreCase = true) ||
            clean.equals("HomeGuard-S3", ignoreCase = true)
    }

    private fun visibleSortName(value: String): String =
        if (isGeneratedName(value)) "~" else value.trim().lowercase()

    private fun normalizeEndpoint(value: String): String = value.trim().trimEnd('/').lowercase()
}
