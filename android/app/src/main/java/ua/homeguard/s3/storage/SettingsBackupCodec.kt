package ua.homeguard.s3.storage

import org.json.JSONObject

object SettingsBackupCodec {
    private const val FORMAT = "homeguard-s3-settings"
    private const val VERSION = 1

    fun encode(settings: AppSettings): String = JSONObject()
        .put("format", FORMAT)
        .put("version", VERSION)
        .put("deviceId", settings.deviceId)
        .put("autoReconnect", settings.autoReconnect)
        .put("remoteAccessEnabled", settings.remoteAccessEnabled)
        .put("cloudBaseUrl", settings.cloudBaseUrl)
        .put("lastKnownLocalUrl", settings.lastKnownLocalUrl)
        .put("localCertificateSha256", settings.localCertificateSha256)
        .put("criticalNotificationsEnabled", settings.criticalNotificationsEnabled)
        .put("statusNotificationsEnabled", settings.statusNotificationsEnabled)
        .put("zoneNotificationsEnabled", settings.zoneNotificationsEnabled)
        .toString(2)

    fun decode(text: String, currentToken: String = ""): AppSettings {
        val json = JSONObject(text)
        require(json.optString("format") == FORMAT) { "Unsupported backup format" }
        require(json.optInt("version") == VERSION) { "Unsupported backup version" }
        return AppSettings(
            deviceId = json.optString("deviceId"),
            apiToken = currentToken,
            autoReconnect = json.optBoolean("autoReconnect", true),
            remoteAccessEnabled = json.optBoolean("remoteAccessEnabled", false),
            cloudBaseUrl = json.optString("cloudBaseUrl"),
            lastKnownLocalUrl = json.optString("lastKnownLocalUrl"),
            localCertificateSha256 = json.optString("localCertificateSha256"),
            criticalNotificationsEnabled = json.optBoolean("criticalNotificationsEnabled", true),
            statusNotificationsEnabled = json.optBoolean("statusNotificationsEnabled", true),
            zoneNotificationsEnabled = json.optBoolean("zoneNotificationsEnabled", true),
        )
    }

    fun suggestedFileName(): String = "homeguard-s3-settings.json"
}
