package ua.homeguard.s3.storage

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.provisioning.SecureTokenStore

class SettingsStore(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences("homeguard_settings", Context.MODE_PRIVATE)
    private val secure = SecureTokenStore(context)
    val settings = MutableStateFlow(load())

    suspend fun update(value: AppSettings) {
        preferences.edit()
            .putString("device_id", value.deviceId)
            .putBoolean("auto_reconnect", value.autoReconnect)
            .putBoolean("remote_access", value.remoteAccessEnabled)
            .putString("cloud_base_url", value.cloudBaseUrl)
            .putString("last_local_url", value.lastKnownLocalUrl)
            .putString("local_cert_sha256", value.localCertificateSha256)
            .apply()
        secure.put("api_token", value.apiToken)
        settings.emit(value)
    }

    suspend fun remember(device: DiscoveredDevice) {
        update(settings.value.copy(deviceId = device.deviceId, lastKnownLocalUrl = device.baseUrl))
    }

    private fun load() = AppSettings(
        deviceId = preferences.getString("device_id", "").orEmpty(),
        apiToken = secure.get("api_token"),
        autoReconnect = preferences.getBoolean("auto_reconnect", true),
        remoteAccessEnabled = preferences.getBoolean("remote_access", false),
        cloudBaseUrl = preferences.getString("cloud_base_url", "").orEmpty(),
        lastKnownLocalUrl = preferences.getString("last_local_url", "").orEmpty(),
        localCertificateSha256 = preferences.getString("local_cert_sha256", "").orEmpty()
    )
}
