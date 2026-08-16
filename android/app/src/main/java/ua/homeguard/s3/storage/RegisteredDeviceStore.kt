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
 * Persistent owner-facing device registry.
 *
 * Invariants are enforced here, not in individual screens:
 *  - one physical controller -> one registry entry;
 *  - owner-assigned names are the only display names we persist;
 *  - technical identity (ID/IP/service name) can never become a display name;
 *  - discovery may refresh technical fields, but never overwrite the owner's name;
 *  - manual records are atomically replaced by the real discovered identity.
 */
class RegisteredDeviceStore(context: Context) {
    companion object {
        @Volatile private var activeStore: RegisteredDeviceStore? = null

        suspend fun markActiveAuthorization(deviceId: String, authorized: Boolean) {
            if (deviceId.isNotBlank()) activeStore?.markAuthorization(deviceId, authorized)
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

    private val _devices = MutableStateFlow(readCanonicalRegistry())
    val devices: StateFlow<List<RegisteredDevice>> = _devices.asStateFlow()

    init {
        activeStore = this
    }

    suspend fun addOrUpdate(device: DiscoveredDevice, requestedName: String? = null) {
        val requested = ownerNameOrNull(requestedName.orEmpty(), device.deviceId, device.baseUrl)
        val current = physicalMatches(_devices.value, device.deviceId, device.baseUrl)
        val previous = preferredOwnerRecord(current)
        val ownerName = requested ?: previous?.let { ownerNameOrNull(it.name, it.deviceId, it.baseUrl) } ?: return

        replacePhysicalDevice(
            deviceId = device.deviceId,
            baseUrl = device.baseUrl,
            replacement = RegisteredDevice(
                deviceId = device.deviceId,
                name = ownerName,
                baseUrl = device.baseUrl,
                lastSeenAtMs = device.seenAtMs,
                authorized = current.firstOrNull { it.deviceId.equals(device.deviceId, true) }?.authorized
                    ?: previous?.authorized
                    ?: true,
            ),
        )
    }

    suspend fun addManual(deviceId: String, baseUrl: String, name: String = "") {
        val current = physicalMatches(_devices.value, deviceId, baseUrl)
        val previous = preferredOwnerRecord(current)
        val ownerName = ownerNameOrNull(name, deviceId, baseUrl)
            ?: previous?.let { ownerNameOrNull(it.name, it.deviceId, it.baseUrl) }
            ?: return

        replacePhysicalDevice(
            deviceId = deviceId,
            baseUrl = baseUrl,
            replacement = RegisteredDevice(
                deviceId = deviceId,
                name = ownerName,
                baseUrl = baseUrl,
                lastSeenAtMs = System.currentTimeMillis(),
                authorized = previous?.authorized ?: true,
            ),
        )
    }

    suspend fun reconcileManual(manualDeviceId: String, discovered: DiscoveredDevice): Boolean {
        if (!manualDeviceId.startsWith("manual-") || discovered.deviceId.isBlank()) return false

        val snapshot = _devices.value
        val manual = snapshot.firstOrNull { it.deviceId == manualDeviceId } ?: return false
        if (!samePhysical(manual, discovered.deviceId, discovered.baseUrl)) return false

        val matches = physicalMatches(snapshot, discovered.deviceId, discovered.baseUrl)
        val real = matches.firstOrNull { it.deviceId.equals(discovered.deviceId, true) }
        val ownerName = ownerNameOrNull(manual.name, manual.deviceId, manual.baseUrl)
            ?: real?.let { ownerNameOrNull(it.name, it.deviceId, it.baseUrl) }
            ?: return false

        replacePhysicalDevice(
            deviceId = discovered.deviceId,
            baseUrl = discovered.baseUrl,
            replacement = RegisteredDevice(
                deviceId = discovered.deviceId,
                name = ownerName,
                baseUrl = discovered.baseUrl,
                lastSeenAtMs = discovered.seenAtMs,
                authorized = real?.authorized ?: manual.authorized,
            ),
        )
        return true
    }

    suspend fun refreshDiscovered(discovered: DiscoveredDevice): Boolean {
        val matches = physicalMatches(_devices.value, discovered.deviceId, discovered.baseUrl)
        if (matches.isEmpty()) return false

        val previous = preferredOwnerRecord(matches) ?: return false
        val ownerName = ownerNameOrNull(previous.name, previous.deviceId, previous.baseUrl) ?: return false
        val exact = matches.firstOrNull { it.deviceId.equals(discovered.deviceId, true) }
        val refreshed = RegisteredDevice(
            deviceId = discovered.deviceId,
            name = ownerName,
            baseUrl = discovered.baseUrl,
            lastSeenAtMs = discovered.seenAtMs,
            authorized = exact?.authorized ?: previous.authorized,
        )

        if (matches.size == 1 && matches.single() == refreshed) return false
        replacePhysicalDevice(discovered.deviceId, discovered.baseUrl, refreshed)
        return true
    }

    suspend fun rename(deviceId: String, name: String) {
        val target = _devices.value.firstOrNull { it.deviceId == deviceId } ?: return
        val ownerName = ownerNameOrNull(name, target.deviceId, target.baseUrl) ?: return
        writeCanonicalRegistry(
            _devices.value.map { candidate ->
                if (samePhysical(candidate, target.deviceId, target.baseUrl)) {
                    candidate.copy(name = ownerName)
                } else {
                    candidate
                }
            },
        )
    }

    suspend fun remove(deviceId: String): Boolean {
        val target = _devices.value.firstOrNull { it.deviceId == deviceId } ?: return false
        writeCanonicalRegistry(
            _devices.value.filterNot { samePhysical(it, target.deviceId, target.baseUrl) },
        )
        return true
    }

    suspend fun markAuthorization(deviceId: String, authorized: Boolean) {
        val target = _devices.value.firstOrNull { it.deviceId == deviceId } ?: return
        writeCanonicalRegistry(
            _devices.value.map { candidate ->
                if (samePhysical(candidate, target.deviceId, target.baseUrl)) {
                    candidate.copy(authorized = authorized)
                } else {
                    candidate
                }
            },
        )
    }

    private suspend fun replacePhysicalDevice(
        deviceId: String,
        baseUrl: String,
        replacement: RegisteredDevice,
    ) {
        writeCanonicalRegistry(
            _devices.value.filterNot { samePhysical(it, deviceId, baseUrl) } + replacement,
        )
    }

    private suspend fun writeCanonicalRegistry(value: List<RegisteredDevice>) {
        val canonical = canonicalize(value)
        preferences.edit().putString("devices", encode(canonical)).apply()
        _devices.emit(canonical)
    }

    private fun readCanonicalRegistry(): List<RegisteredDevice> = runCatching {
        val raw = preferences.getString("devices", "[]").orEmpty()
        val array = JSONArray(raw)
        val loaded = buildList {
            for (index in 0 until array.length()) {
                val item = array.getJSONObject(index)
                val deviceId = item.optString("device_id").trim()
                if (deviceId.isBlank()) continue
                val baseUrl = item.optString("base_url").trim()
                val ownerName = ownerNameOrNull(item.optString("name", ""), deviceId, baseUrl).orEmpty()
                add(
                    RegisteredDevice(
                        deviceId = deviceId,
                        name = ownerName,
                        baseUrl = baseUrl,
                        lastSeenAtMs = item.optLong("last_seen_at_ms"),
                        authorized = item.optBoolean("authorized", true),
                    ),
                )
            }
        }
        canonicalize(loaded)
    }.getOrDefault(emptyList())

    private fun encode(value: List<RegisteredDevice>): String {
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
        return json.toString()
    }

    private fun canonicalize(value: List<RegisteredDevice>): List<RegisteredDevice> {
        val groups = mutableListOf<MutableList<RegisteredDevice>>()
        value.forEach { candidate ->
            val group = groups.firstOrNull { members ->
                members.any { samePhysical(it, candidate.deviceId, candidate.baseUrl) }
            }
            if (group == null) groups += mutableListOf(candidate) else group += candidate
        }

        return groups.mapNotNull(::mergePhysicalGroup)
            .sortedWith(compareBy<RegisteredDevice> { visibleSortName(it) }.thenBy { it.deviceId.lowercase() })
    }

    private fun mergePhysicalGroup(group: List<RegisteredDevice>): RegisteredDevice? {
        if (group.isEmpty()) return null
        val latest = group.maxByOrNull { it.lastSeenAtMs } ?: return null
        val named = preferredOwnerRecord(group)
        val ownerName = named?.let { ownerNameOrNull(it.name, it.deviceId, it.baseUrl) }.orEmpty()

        // A legacy unnamed record may remain internally so discovery can reconcile it,
        // but no technical identifier is promoted into a visible name.
        return latest.copy(
            name = ownerName,
            authorized = group.all { it.authorized },
        )
    }

    private fun preferredOwnerRecord(records: List<RegisteredDevice>): RegisteredDevice? =
        records
            .filter { ownerNameOrNull(it.name, it.deviceId, it.baseUrl) != null }
            .maxByOrNull { it.lastSeenAtMs }
            ?: records.maxByOrNull { it.lastSeenAtMs }

    private fun physicalMatches(
        value: List<RegisteredDevice>,
        deviceId: String,
        baseUrl: String,
    ): List<RegisteredDevice> = value.filter { samePhysical(it, deviceId, baseUrl) }

    private fun samePhysical(
        registered: RegisteredDevice,
        deviceId: String,
        baseUrl: String,
    ): Boolean = DeviceIdentity.samePhysicalDevice(
        registered.deviceId,
        registered.baseUrl,
        deviceId,
        baseUrl,
    )

    private fun ownerNameOrNull(value: String, deviceId: String, baseUrl: String): String? {
        val clean = value.trim().take(40)
        if (clean.isBlank()) return null
        if (clean.equals("HomeGuard", true) || clean.equals("HomeGuard-S3", true)) return null
        if (deviceId.isNotBlank() && clean.equals(deviceId.trim(), true)) return null

        val endpoint = baseUrl.trim().trimEnd('/').lowercase()
        if (endpoint.isNotBlank() && clean.equals(endpoint, true)) return null
        val host = DeviceIdentity.endpointHost(endpoint)
        if (host.isNotBlank() && clean.equals(host, true)) return null
        return clean
    }

    private fun visibleSortName(device: RegisteredDevice): String =
        ownerNameOrNull(device.name, device.deviceId, device.baseUrl)?.lowercase() ?: "~"
}
