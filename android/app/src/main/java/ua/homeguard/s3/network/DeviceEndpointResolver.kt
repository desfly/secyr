package ua.homeguard.s3.network

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.storage.RegisteredDeviceStore
import ua.homeguard.s3.storage.SettingsStore

class DeviceEndpointResolver(
    settings: SettingsStore,
    discovery: LocalDiscoveryCoordinator,
    scope: CoroutineScope
) {
    val endpoint: StateFlow<DeviceEndpoint> = combine(settings.settings, discovery.devices) { config, devices ->
        val eligible = devices.filter { it.apiVersion == 1 }
        val directLocal = eligible.firstOrNull { it.deviceId == config.deviceId }
        val manualLocal = if (config.deviceId.startsWith("manual-") && config.lastKnownLocalUrl.isNotBlank()) {
            val expected = EndpointUrlBuilder.normalizeBaseUrl(config.lastKnownLocalUrl)
            eligible.firstOrNull { EndpointUrlBuilder.normalizeBaseUrl(it.baseUrl) == expected }
        } else {
            null
        }
        val local = directLocal
            ?: manualLocal
            ?: if (config.deviceId.isBlank() && eligible.size == 1) eligible.first() else null

        if (manualLocal != null) {
            scope.launch {
                if (RegisteredDeviceStore.reconcileActiveManual(config.deviceId, manualLocal)) {
                    settings.remember(manualLocal)
                }
            }
        }

        when (selectControlPath(
            EndpointAvailability(
                hasDeviceId = config.deviceId.isNotBlank() || local != null,
                matchingLocalFound = local != null,
                matchingLocalSecure = local?.secure == true,
                matchingLocalApiVersion = local?.apiVersion ?: 0,
                hasLastKnownLocal = config.lastKnownLocalUrl.isNotBlank(),
                remoteAccessEnabled = config.remoteAccessEnabled,
                hasCloudBaseUrl = config.cloudBaseUrl.isNotBlank(),
            ),
        )) {
            ControlPath.LOCAL -> {
                val selected = checkNotNull(local) { "LOCAL route selected without a matching device" }
                val base = EndpointUrlBuilder.normalizeBaseUrl(selected.baseUrl)
                DeviceEndpoint(
                    deviceId = selected.deviceId,
                    apiBaseUrl = base,
                    websocketUrl = EndpointUrlBuilder.websocketUrl(base),
                    path = ControlPath.LOCAL,
                    certificateSha256 = if (selected.secure) config.localCertificateSha256 else "",
                )
            }

            ControlPath.LAST_KNOWN_LOCAL -> {
                val base = EndpointUrlBuilder.normalizeBaseUrl(config.lastKnownLocalUrl)
                DeviceEndpoint(
                    deviceId = config.deviceId,
                    apiBaseUrl = base,
                    websocketUrl = EndpointUrlBuilder.websocketUrl(base),
                    path = ControlPath.LAST_KNOWN_LOCAL,
                    certificateSha256 = if (base.startsWith("https://", true)) config.localCertificateSha256 else "",
                )
            }

            ControlPath.CLOUD -> {
                val base = EndpointUrlBuilder.cloudDeviceBase(config.cloudBaseUrl, config.deviceId)
                DeviceEndpoint(
                    deviceId = config.deviceId,
                    apiBaseUrl = base,
                    websocketUrl = EndpointUrlBuilder.websocketUrl(base),
                    path = ControlPath.CLOUD,
                )
            }

            ControlPath.OFFLINE -> DeviceEndpoint(config.deviceId, "", "", ControlPath.OFFLINE)
        }
    }.stateIn(scope, SharingStarted.Eagerly, DeviceEndpoint("", "", "", ControlPath.OFFLINE))
}
