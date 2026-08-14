package ua.homeguard.s3.network

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.storage.SettingsStore
import java.net.URLEncoder

class DeviceEndpointResolver(
    settings: SettingsStore,
    discovery: LocalDiscoveryCoordinator,
    scope: CoroutineScope
) {
    val endpoint: StateFlow<DeviceEndpoint> = combine(settings.settings, discovery.devices) { config, devices ->
        val eligible = devices.filter { it.apiVersion == 1 }
        val local = eligible.firstOrNull { it.deviceId == config.deviceId }
            ?: if (config.deviceId.isBlank() && eligible.size == 1) eligible.first() else null
        when (selectControlPath(EndpointAvailability(
            hasDeviceId = config.deviceId.isNotBlank() || local != null,
            matchingLocalFound = local != null,
            matchingLocalSecure = local?.secure == true,
            matchingLocalApiVersion = local?.apiVersion ?: 0,
            hasLastKnownLocal = config.lastKnownLocalUrl.isNotBlank(),
            remoteAccessEnabled = config.remoteAccessEnabled,
            hasCloudBaseUrl = config.cloudBaseUrl.isNotBlank()
        ))) {
            ControlPath.LOCAL -> {
                val selected = checkNotNull(local) { "LOCAL route selected without a matching device" }
                DeviceEndpoint(
                    deviceId = selected.deviceId,
                    apiBaseUrl = selected.baseUrl,
                    websocketUrl = selected.baseUrl.replaceFirst("http", "ws") + RuntimeApiContract.TELEMETRY_PATH,
                    path = ControlPath.LOCAL,
                    certificateSha256 = if (selected.secure) config.localCertificateSha256 else ""
                )
            }
            ControlPath.LAST_KNOWN_LOCAL -> DeviceEndpoint(
                deviceId = config.deviceId,
                apiBaseUrl = config.lastKnownLocalUrl.trimEnd('/'),
                websocketUrl = config.lastKnownLocalUrl.replaceFirst("http", "ws").trimEnd('/') + RuntimeApiContract.TELEMETRY_PATH,
                path = ControlPath.LAST_KNOWN_LOCAL,
                certificateSha256 = if (config.lastKnownLocalUrl.startsWith("https://", true)) config.localCertificateSha256 else ""
            )
            ControlPath.CLOUD -> {
                val id = URLEncoder.encode(config.deviceId, Charsets.UTF_8.name())
                val base = config.cloudBaseUrl.trimEnd('/') + "/v1/devices/$id"
                DeviceEndpoint(config.deviceId, base, base.replaceFirst("http", "ws") + RuntimeApiContract.TELEMETRY_PATH, ControlPath.CLOUD)
            }
            ControlPath.OFFLINE -> DeviceEndpoint(config.deviceId, "", "", ControlPath.OFFLINE)
        }
    }.stateIn(scope, SharingStarted.Eagerly, DeviceEndpoint("", "", "", ControlPath.OFFLINE))
}
