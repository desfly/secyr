package ua.homeguard.s3.storage

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.provisioning.SecureTokenStore

internal data class DeviceBoundSecrets(
    val apiToken: String,
    val telemetryToken: String,
)

internal fun normalizeDeviceBoundSecrets(current: AppSettings, incoming: AppSettings): DeviceBoundSecrets {
    val deviceChanged = !current.deviceId.equals(incoming.deviceId, ignoreCase = true)
    if (!deviceChanged) {
        return DeviceBoundSecrets(
            apiToken = incoming.apiToken.ifBlank { current.apiToken },
            telemetryToken = incoming.telemetryToken.ifBlank { current.telemetryToken },
        )
    }

    // API/telemetry tokens are controller-bound. A normal selection change is built
    // from current.copy(...), so carrying the same secret value into another device
    // is stale credential reuse and must be cleared. Provisioning is allowed to set a
    // genuinely new non-empty token while changing the selected device.
    return DeviceBoundSecrets(
        apiToken = incoming.apiToken.takeIf { it.isNotBlank() && it != current.apiToken }.orEmpty(),
        telemetryToken = incoming.telemetryToken.takeIf { it.isNotBlank() && it != current.telemetryToken }.orEmpty(),
    )
}

class SettingsStore(context: Context) {
    private val preferences = context.applicationContext.getSharedPreferences("homeguard_settings", Context.MODE_PRIVATE)
    private val secure = SecureTokenStore(context)
    val settings = MutableStateFlow(load())

    suspend fun update(value: AppSettings) {
        val current = settings.value
        val secrets = normalizeDeviceBoundSecrets(current, value)
        val normalized = if (!current.deviceId.equals(value.deviceId, ignoreCase = true)) {
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
                apiToken = secrets.apiToken,
                telemetryToken = secrets.telemetryToken,
            )
        } else {
            value.copy(
                apiToken = secrets.apiToken,
                telemetryToken = secrets.telemetryToken,
            )
        }

        if (normalized == current) return

        preferences.edit()
            .putString("device_id", normalized.deviceId)
            .putBoolean("auto_reconnect", normalized.autoReconnect)
            .putBoolean("remote_access", normalized.remoteAccessEnabled)
            .putString("cloud_base_url", normalized.cloudBaseUrl)
            .putString("last_local_url", normalized.lastKnownLocalUrl)
            .putString("local_cert_sha256", normalized.localCertificateSha256)
            .putBoolean("notifications_critical", normalized.criticalNotificationsEnabled)
            .putBoolean("notifications_status", normalized.statusNotificationsEnabled)
            .putBoolean("notifications_zones", normalized.zoneNotificationsEnabled)
            .apply()
        if (normalized.apiToken != current.apiToken) secure.put("api_token", normalized.apiToken)
        if (normalized.telemetryToken != current.telemetryToken) secure.put("telemetry_token", normalized.telemetryToken)
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

    private fun load() = AppSettings(
        deviceId = preferences.getString("device_id", "").orEmpty(),
        apiToken = secure.get("api_token"),
        telemetryToken = secure.get("telemetry_token"),
        autoReconnect = preferences.getBoolean("auto_reconnect", true),
        remoteAccessEnabled = preferences.getBoolean("remote_access", false),
        cloudBaseUrl = preferences.getString("cloud_base_url", "").orEmpty(),
        lastKnownLocalUrl = preferences.getString("last_local_url", "").orEmpty(),
        localCertificateSha256 = preferences.getString("local_cert_sha256", "").orEmpty(),
        criticalNotificationsEnabled = preferences.getBoolean("notifications_critical", true),
        statusNotificationsEnabled = preferences.getBoolean("notifications_status", true),
        zoneNotificationsEnabled = preferences.getBoolean("notifications_zones", true),
    )
}
