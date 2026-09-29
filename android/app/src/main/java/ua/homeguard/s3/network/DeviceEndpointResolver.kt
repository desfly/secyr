package ua.homeguard.s3.network

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.launch
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.DiscoverySource
import ua.homeguard.s3.storage.RegisteredDeviceStore
import ua.homeguard.s3.storage.SettingsStore

class DeviceEndpointResolver(
    settings: SettingsStore,
    discovery: LocalDiscoveryCoordinator,
    scope: CoroutineScope
) {
    val endpoint: StateFlow<DeviceEndpoint> = combine(settings.settings, discovery.routeCandidates) { config, routes ->
        val eligible = routes.filter { it.apiVersion == 1 }
        val rememberedUrl = config.lastKnownLocalUrl
            .takeIf { it.isNotBlank() }
            ?.let(EndpointUrlBuilder::normalizeBaseUrl)
            .orEmpty()

        val controllerRoutes = eligible.filter { candidate ->
            candidate.deviceId.equals(config.deviceId, ignoreCase = true)
        }

        val rememberedDirect = controllerRoutes.firstOrNull {
            EndpointUrlBuilder.normalizeBaseUrl(it.baseUrl) == rememberedUrl
        }

        val directLocal = rememberedDirect ?: bestRoute(controllerRoutes)

        val manualLocal = if (config.deviceId.startsWith("manual-") && rememberedUrl.isNotBlank()) {
            eligible.firstOrNull { EndpointUrlBuilder.normalizeBaseUrl(it.baseUrl) == rememberedUrl }
        } else {
            null
        }

        val local = directLocal
            ?: manualLocal
            ?: if (config.deviceId.isBlank()) {
                val groups = eligible.groupBy { it.deviceId.lowercase() }
                if (groups.size == 1) bestRoute(groups.values.first()) else null
            } else {
                null
            }

        if (manualLocal != null) {
            scope.launch {
                if (RegisteredDeviceStore.reconcileActiveManual(config.deviceId, manualLocal)) {
                    settings.remember(manualLocal)
                }
            }
        }

        // Keep the currently working .252/.253 route stable while it is still
        // discoverable. Only remember a different route when the previous one
        // disappeared and failover actually happened.
        if (directLocal != null) {
            val discoveredUrl = EndpointUrlBuilder.normalizeBaseUrl(directLocal.baseUrl)
            if (discoveredUrl != rememberedUrl) {
                scope.launch {
                    RegisteredDeviceStore.refreshActiveDiscovered(directLocal)
                    settings.remember(directLocal)
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

    private fun bestRoute(routes: List<DiscoveredDevice>): DiscoveredDevice? =
        routes.maxWithOrNull(
            compareBy<DiscoveredDevice> { it.seenAtMs }
                .thenBy {
                    when (it.source) {
                        DiscoverySource.MDNS -> 2
                        DiscoverySource.UDP -> 1
                        DiscoverySource.HTTP -> 0
                    }
                },
        )
}
