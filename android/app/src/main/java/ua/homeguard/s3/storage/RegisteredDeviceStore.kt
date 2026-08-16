package ua.homeguard.s3.storage

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.json.JSONArray
import org.json.JSONObject
import ua.homeguard.s3.model.DeviceIdentity
import ua.homeguard.s3.model.DiscoveredDevice

data class RegisteredDevice(
    val deviceId: String,
    val name: String,
    val baseUrl: String,
    val lastSeenAtMs: Long,
    val authorized: Boolean = true,
)

/**
 * Canonical persistent registry for owner-visible devices.
 *
 * Rules:
 *  - one physical controller is stored once;
 *  - only an owner-assigned friendly name is visible/persisted;
 *  - discovery refreshes identity/address/last-seen but never replaces that name;
 *  - manual and discovered identities reconcile into one canonical record;
 *  - technical values (service name, ID, IP, endpoint) are never display names.
 */
class RegisteredDeviceStore(context: Context) {
    companion object {
        @Volatile private var activeStore: RegisteredDeviceStore? = null

        suspend fun markActiveAuthorization(deviceId: String, authorized: Boolean) {
            if (deviceId.isNotBlank()) activeStore?.markAuthorization(deviceId, authorized = authorized)
        }

        suspend fun reconcileActiveManual(manualDeviceId: String, discovered: DiscoveredDevice): Boolean =
            activeStore?.reconcileManual(manualDeviceId, discovered) ?: false

        suspend fun refreshActiveDiscovered(discovered: DiscoveredDevice): Boolean =
            activeStore?.refreshDiscovered(discovered) ?: false

        suspend fun removeActive(deviceId: String): Boolean =
            if (deviceId.isBlank()) false else activeStore?.remove(deviceId) ?: false
    }

    private val preferences = context.applicationContext.getSharedPreferences(
        "homeguard_devices",
        Context.MODE_PRIVATE,
    )

    private val _devices = MutableStateFlow(load())
    val devices: StateFlow<List<RegisteredDevice>> = _devices.asStateFlow()

    init {
        activeStore = this
    }

    suspend fun addOrUpdate(device: DiscoveredDevice, requestedName: String? = null) {
        val current = _devices.value.toMutableList()
        val matching = current.filter { sameDevice(it, device) }
        val exact = matching.firstOrNull { it.deviceId.equals(device.deviceId, ignoreCase = true) }
        val named = matching.firstOrNull { !isGeneratedName(it.name, it.deviceId, it.baseUrl) }
        val previous = named ?: exact ?: matching.maxByOrNull { it.lastSeenAtMs }

        val requested = requestedName?.trim()?.take(40).orEmpty()
        val displayName = requested.ifBlank { previous?.name.orEmpty() }
        if (displayName.isBlank() || isGeneratedName(displayName, device.deviceId, device.baseUrl)) return

        val replacement = RegisteredDevice(
            deviceId = device.deviceId,
            name = displayName,
            baseUrl = device.baseUrl,
            lastSeenAtMs = device.seenAtMs,
            authorized = exact?.authorized ?: previous?.authorized ?: true,
        )

        current.removeAll { sameDevice(it, device) }
        current += replacement
        persist(current)
    }

    suspend fun addManual(deviceId: String, baseUrl: String, name: String = "") {
        val current = _devices.value.toMutableList()
        val matching = current.filter {
            DeviceIdentity.samePhysicalDevice(it.deviceId, it.baseUrl, deviceId, baseUrl)
        }
        val named = matching.firstOrNull { !isGeneratedName(it.name, it.deviceId, it.baseUrl) }
        val previous = named ?: matching.maxByOrNull { it.lastSeenAtMs }

        val requested = name.trim().take(40)
        val displayName = requested.ifBlank { previous?.name.orEmpty() }
        if (displayName.isBlank() || isGeneratedName(displayName, deviceId, baseUrl)) return

        current.removeAll {
            DeviceIdentity.samePhysicalDevice(it.deviceId, it.baseUrl, deviceId, baseUrl)
        }
        current += RegisteredDevice(
            deviceId = deviceId,
            name = displayName,
            baseUrl = baseUrl,
            lastSeenAtMs = System.currentTimeMillis(),
            authorized = previous?.authorized ?: true,
        )
        persist(current)
    }

    suspend fun reconcileManual(manualDeviceId: String, discovered: DiscoveredDevice): Boolean {
        if (!manualDeviceId.startsWith("manual-") || discovered.deviceId.isBlank()) return false

        val current = _devices.value.toMutableList()
        val manual = current.firstOrNull { it.deviceId == manualDeviceId } ?: return false
        val matching = current.filter { sameDevice(it, discovered) || it.deviceId == manualDeviceId }
        val exact = matching.firstOrNull { it.deviceId.equals(discovered.deviceId, ignoreCase = true) }
        val named = matching.firstOrNull { !isGeneratedName(it.name, it.deviceId, it.baseUrl) }
        val previous = named ?: exact ?: manual
        val ownerName = previous.name.trim().take(40)
        if (ownerName.isBlank() || isGeneratedName(ownerName, previous.deviceId, previous.baseUrl)) return false

        current.removeAll { it.deviceId == manualDeviceId || sameDevice(it, discovered) }
        current += RegisteredDevice(
            deviceId = discovered.deviceId,
            name = ownerName,
            baseUrl = discovered.baseUrl,
            lastSeenAtMs = discovered.seenAtMs,
            authorized = exact?.authorized ?: manual.authorized,
        )
        persist(current)
        return true
    }

    suspend fun refreshDiscovered(discovered: DiscoveredDevice): Boolean {
        val current = _devices.value.toMutableList()
        val matching = current.filter { sameDevice(it, discovered) }
        if (matching.isEmpty()) return false

        val exact = matching.firstOrNull { it.deviceId.equals(discovered.deviceId, ignoreCase = true) }
        val named = matching.firstOrNull { !isGeneratedName(it.name, it.deviceId, it.baseUrl) }
        val previous = named ?: exact ?: matching.maxByOrNull { it.lastSeenAtMs } ?: return false
        if (isGeneratedName(previous.name, previous.deviceId, previous.baseUrl)) return false

        val replacement = RegisteredDevice(
            deviceId = discovered.deviceId,
            name = previous.name,
            baseUrl = discovered.baseUrl,
            lastSeenAtMs = discovered.seenAtMs,
            authorized = exact?.authorized ?: previous.authorized,
        )

        if (matching.size == 1 && matching.single() == replacement) return false
        current.removeAll { sameDevice(it, discovered) }
        current += replacement
        persist(current)
        return true
    }

    suspend fun rename(deviceId: String, name: String) {
        val target = _devices.value.firstOrNull { it.deviceId == deviceId } ?: return
        val clean = name.trim().take(40)
        if (clean.isBlank() || isGeneratedName(clean, target.deviceId, target.baseUrl)) return

        persist(
            _devices.value.map { candidate ->
                if (DeviceIdentity.samePhysicalDevice(
                        candidate.deviceId,
                        candidate.baseUrl,
                        target.deviceId,
                        target.baseUrl,
                    )
                ) {
                    candidate.copy(name = clean)
                } else {
                    candidate
                }
            },
        )
    }

    suspend fun remove(deviceId: String): Boolean {
        val target = _devices.value.firstOrNull { it.deviceId == deviceId } ?: return false
        persist(
            _devices.value.filterNot {
                DeviceIdentity.samePhysicalDevice(
                    it.deviceId,
                    it.baseUrl,
                    target.deviceId,
                    target.baseUrl,
                )
            },
        )
        return true
    }

    suspend fun markAuthorization(deviceId: String, baseUrl: String = "", authorized: Boolean) {
        val target = _devices.value.firstOrNull { candidate ->
            DeviceIdentity.samePhysicalDevice(
                candidate.deviceId,
                candidate.baseUrl,
                deviceId,
                baseUrl,
            )
        } ?: _devices.value.firstOrNull { it.deviceId.equals(deviceId, ignoreCase = true) }
        ?: return

        persist(
            _devices.value.map { candidate ->
                if (DeviceIdentity.samePhysicalDevice(
                        candidate.deviceId,
                        candidate.baseUrl,
                        target.deviceId,
                        target.baseUrl,
                    )
                ) {
                    candidate.copy(authorized = authorized)
                } else {
                    candidate
                }
            },
        )
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
        _devices.emit(clean.sortedBy { visibleSortName(it) })
    }

    private fun load(): List<RegisteredDevice> = runCatching {
        val raw = preferences.getString("devices", "[]").orEmpty()
        val array = JSONArray(raw)
        val loaded = buildList {
            for (index in 0 until array.length()) {
                val item = array.getJSONObject(index)
                val id = item.optString("device_id").trim()
                if (id.isBlank()) continue
                val baseUrl = item.optString("base_url").trim()
                val storedName = item.optString("name", "").trim().take(40)
                add(
                    RegisteredDevice(
                        deviceId = id,
                        name = if (isGeneratedName(storedName, id, baseUrl)) "" else storedName,
                        baseUrl = baseUrl,
                        lastSeenAtMs = item.optLong("last_seen_at_ms"),
                        authorized = item.optBoolean("authorized", true),
                    ),
                )
            }
        }
        deduplicate(loaded).sortedBy { visibleSortName(it) }
    }.getOrDefault(emptyList())

    private fun deduplicate(value: List<RegisteredDevice>): List<RegisteredDevice> {
        val result = mutableListOf<RegisteredDevice>()
        value.sortedByDescending { it.lastSeenAtMs }.forEach { candidate ->
            val duplicateIndex = result.indexOfFirst { existing ->
                DeviceIdentity.samePhysicalDevice(
                    existing.deviceId,
                    existing.baseUrl,
                    candidate.deviceId,
                    candidate.baseUrl,
                )
            }
            if (duplicateIndex < 0) {
                result += candidate
                return@forEach
            }

            val existing = result[duplicateIndex]
            val ownerName = when {
                !isGeneratedName(existing.name, existing.deviceId, existing.baseUrl) -> existing.name
                !isGeneratedName(candidate.name, candidate.deviceId, candidate.baseUrl) -> candidate.name
                else -> ""
            }
            val latest = if (existing.lastSeenAtMs >= candidate.lastSeenAtMs) existing else candidate
            result[duplicateIndex] = latest.copy(
                name = ownerName,
                authorized = existing.authorized && candidate.authorized,
            )
        }
        return result
    }

    private fun sameDevice(registered: RegisteredDevice, discovered: DiscoveredDevice): Boolean =
        DeviceIdentity.samePhysicalDevice(
            registered.deviceId,
            registered.baseUrl,
            discovered.deviceId,
            discovered.baseUrl,
        )

    private fun isGeneratedName(value: String, deviceId: String = "", baseUrl: String = ""): Boolean {
        val clean = value.trim()
        if (clean.isBlank()) return true
        if (clean.equals("HomeGuard", ignoreCase = true)) return true
        if (clean.equals("HomeGuard-S3", ignoreCase = true)) return true
        if (deviceId.isNotBlank() && clean.equals(deviceId.trim(), ignoreCase = true)) return true

        val endpoint = baseUrl.trim().trimEnd('/').lowercase()
        if (endpoint.isNotBlank() && clean.equals(endpoint, ignoreCase = true)) return true
        val host = DeviceIdentity.endpointHost(endpoint)
        if (host.isNotBlank() && clean.equals(host, ignoreCase = true)) return true
        return false
    }

    private fun visibleSortName(device: RegisteredDevice): String =
        if (isGeneratedName(device.name, device.deviceId, device.baseUrl)) "~" else device.name.trim().lowercase()
}