package ua.homeguard.s3.storage

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.provisioning.SecureTokenStore

class SettingsStore(context: Context) {
    internal val appContext: Context = context.applicationContext
    private val preferences = appContext.getSharedPreferences("homeguard_settings", Context.MODE_PRIVATE)
    private val secure = SecureTokenStore(appContext)
    val settings = MutableStateFlow(load())

    suspend fun update(value: AppSettings) {
        val current = settings.value
        val normalized = if (current.deviceId != value.deviceId) {
            val selection = DeviceSelectionPolicy.select(
                currentDeviceId = current.deviceId,
                currentLocalUrl = current.lastKnownLocalUrl,
                currentCertificateSha256 = current.localCertificateSha256,
                nextDeviceId = value.deviceId,
                confirmedLocalUrl = value.lastKnownLocalUrl.takeIf { it.isNotBlank() },
            )
            value.copy(
                deviceId = selection.deviceId,
                lastKnownLocalUrl = selection.lastKnownLocalUrl,
                localCertificateSha256 = selection.localCertificateSha256,
            )
        } else {
            value
        }

        preferences.edit()
            .putString("device_id", normalized.deviceId)
            .putBoolean("auto_reconnect", normalized.autoReconnect)
            .putBoolean("remote_access", normalized.remoteAccessEnabled)
            .putString("cloud_base_url", normalized.cloudBaseUrl)
            .putString("last_local_url", normalized.lastKnownLocalUrl)
            .putString("local_cert_sha256", normalized.localCertificateSha256)
            .putString("mqtt_broker_uri", normalized.mqttBrokerUri)
            .putString("mqtt_username", normalized.mqttUsername)
            .putBoolean("notifications_critical", normalized.criticalNotificationsEnabled)
            .putBoolean("notifications_status", normalized.statusNotificationsEnabled)
            .putBoolean("notifications_zones", normalized.zoneNotificationsEnabled)
            .apply()
        secure.put("api_token", normalized.apiToken)
        // Telemetry tickets are short-lived, single-use WebSocket handshake
        // credentials. Persisting one across process restarts guarantees a stale
        // 401 on the next launch and can incorrectly mark the controller as
        // unauthorized. Keep the current ticket only in memory.
        secure.put("telemetry_token", "")
        settings.emit(normalized)
    }

    suspend fun selectDevice(deviceId: String, confirmedLocalUrl: String? = null) {
        val current = settings.value
        val selection = DeviceSelectionPolicy.select(
            currentDeviceId = current.deviceId,
            currentLocalUrl = current.lastKnownLocalUrl,
            currentCertificateSha256 = current.localCertificateSha256,
            nextDeviceId = deviceId,
            confirmedLocalUrl = confirmedLocalUrl,
        )
        update(
            current.copy(
                deviceId = selection.deviceId,
                lastKnownLocalUrl = selection.lastKnownLocalUrl,
                localCertificateSha256 = selection.localCertificateSha256,
            ),
        )
    }

    suspend fun remember(device: DiscoveredDevice) {
        selectDevice(device.deviceId, device.baseUrl)
    }

    data class SavedLogin(val actor: String, val pin: String)

    private fun loginKey(kind: String, deviceId: String): String =
        "saved_login_" + kind + "_" + deviceId

    fun saveLogin(deviceId: String = settings.value.deviceId, actor: String, pin: String) {
        if (deviceId.isBlank() || actor.isBlank() || pin.length !in 4..12 || !pin.all(Char::isDigit)) return
        secure.put(loginKey("actor", deviceId), actor)
        secure.put(loginKey("pin", deviceId), pin)
    }

    fun savedLogin(deviceId: String = settings.value.deviceId): SavedLogin? {
        if (deviceId.isBlank()) return null
        val actor = secure.get(loginKey("actor", deviceId))
        val pin = secure.get(loginKey("pin", deviceId))
        return if (actor.isNotBlank() && pin.length in 4..12 && pin.all(Char::isDigit)) SavedLogin(actor, pin) else null
    }

    fun clearSavedLogin(deviceId: String = settings.value.deviceId) {
        if (deviceId.isBlank()) return
        secure.put(loginKey("actor", deviceId), "")
        secure.put(loginKey("pin", deviceId), "")
    }

    suspend fun saveMqttClientConfig(brokerUri: String, username: String, password: String) {
        secure.put("mqtt_password", password)
        update(
            settings.value.copy(
                mqttBrokerUri = brokerUri.trim(),
                mqttUsername = username.trim(),
            ),
        )
    }

    fun mqttPassword(): String = secure.get("mqtt_password")

    private fun load() = AppSettings(
        deviceId = preferences.getString("device_id", "").orEmpty(),
        apiToken = secure.get("api_token"),
        // Never restore a one-shot telemetry ticket from durable storage.
        telemetryToken = "",
        autoReconnect = preferences.getBoolean("auto_reconnect", true),
        remoteAccessEnabled = preferences.getBoolean("remote_access", false),
        cloudBaseUrl = preferences.getString("cloud_base_url", "").orEmpty(),
        lastKnownLocalUrl = preferences.getString("last_local_url", "").orEmpty(),
        localCertificateSha256 = preferences.getString("local_cert_sha256", "").orEmpty(),
        mqttBrokerUri = preferences.getString("mqtt_broker_uri", "").orEmpty(),
        mqttUsername = preferences.getString("mqtt_username", "").orEmpty(),
        criticalNotificationsEnabled = preferences.getBoolean("notifications_critical", true),
        statusNotificationsEnabled = preferences.getBoolean("notifications_status", true),
        zoneNotificationsEnabled = preferences.getBoolean("notifications_zones", true),
    )
}
