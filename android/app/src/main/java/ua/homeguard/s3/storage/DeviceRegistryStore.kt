package ua.homeguard.s3.storage

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import ua.homeguard.s3.model.DeviceAccessState
import ua.homeguard.s3.model.RegisteredDevice

class DeviceRegistryStore(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences("homeguard_device_registry", Context.MODE_PRIVATE)
    private val _devices = MutableStateFlow(load())
    val devices: StateFlow<List<RegisteredDevice>> = _devices

    fun migrateLegacy(deviceId: String, lastKnownUrl: String) {
        if (deviceId.isBlank() || _devices.value.any { it.deviceId == deviceId }) return
        upsert(
            RegisteredDevice(
                deviceId = deviceId,
                name = "HomeGuard",
                lastKnownUrl = lastKnownUrl,
            )
        )
    }

    fun upsert(device: RegisteredDevice) {
        val updated = _devices.value
            .filterNot { it.deviceId == device.deviceId } + device
        persist(updated)
    }

    fun rename(deviceId: String, name: String) {
        val clean = name.trim()
        if (clean.isBlank()) return
        persist(_devices.value.map { if (it.deviceId == deviceId) it.copy(name = clean) else it })
    }

    fun setAccess(deviceId: String, state: DeviceAccessState) {
        persist(_devices.value.map { if (it.deviceId == deviceId) it.copy(accessState = state) else it })
    }

    fun remove(deviceId: String) {
        persist(_devices.value.filterNot { it.deviceId == deviceId })
    }

    private fun persist(devices: List<RegisteredDevice>) {
        val encoded = devices.joinToString("\n") { device ->
            listOf(
                escape(device.deviceId),
                escape(device.name),
                escape(device.lastKnownUrl),
                device.accessState.name,
                device.addedAtMs.toString(),
            ).joinToString("|")
        }
        preferences.edit().putString(KEY_DEVICES, encoded).apply()
        _devices.value = devices.sortedBy { it.name.lowercase() }
    }

    private fun load(): List<RegisteredDevice> {
        val encoded = preferences.getString(KEY_DEVICES, "").orEmpty()
        if (encoded.isBlank()) return emptyList()
        return encoded.lineSequence().mapNotNull { line ->
            val parts = splitEscaped(line)
            if (parts.size < 5) return@mapNotNull null
            RegisteredDevice(
                deviceId = unescape(parts[0]),
                name = unescape(parts[1]),
                lastKnownUrl = unescape(parts[2]),
                accessState = runCatching { DeviceAccessState.valueOf(parts[3]) }.getOrDefault(DeviceAccessState.ACTIVE),
                addedAtMs = parts[4].toLongOrNull() ?: System.currentTimeMillis(),
            )
        }.sortedBy { it.name.lowercase() }.toList()
    }

    private fun escape(value: String): String = value
        .replace("\\", "\\\\")
        .replace("|", "\\p")
        .replace("\n", "\\n")

    private fun unescape(value: String): String = value
        .replace("\\n", "\n")
        .replace("\\p", "|")
        .replace("\\\\", "\\")

    private fun splitEscaped(line: String): List<String> {
        val result = mutableListOf<String>()
        val current = StringBuilder()
        var escaped = false
        for (char in line) {
            if (escaped) {
                current.append('\\').append(char)
                escaped = false
            } else if (char == '\\') {
                escaped = true
            } else if (char == '|') {
                result += current.toString()
                current.clear()
            } else {
                current.append(char)
            }
        }
        if (escaped) current.append('\\')
        result += current.toString()
        return result
    }

    companion object {
        private const val KEY_DEVICES = "devices"
    }
}
