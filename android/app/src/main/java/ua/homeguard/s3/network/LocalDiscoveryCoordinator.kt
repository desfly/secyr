package ua.homeguard.s3.network

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.DiscoverySource

class LocalDiscoveryCoordinator(context: Context, private val scope: CoroutineScope) {
    private val appContext = context.applicationContext
    private val nsd = NsdDeviceDiscovery(appContext)
    private val udp = UdpDeviceDiscovery(appContext, scope)
    private val http = HttpSubnetDiscovery(appContext)

    val scanStatus: StateFlow<UdpDeviceDiscovery.ScanStatus> = udp.status
    val isScanning: StateFlow<Boolean> = udp.status
        .map { it.phase == "sending" || it.phase == "listening" }
        .stateIn(scope, SharingStarted.Eagerly, false)

    val devices: StateFlow<List<DiscoveredDevice>> = combine(nsd.devices, udp.devices, http.devices) { mdns, udpFallback, httpFallback ->
        (mdns + udpFallback + httpFallback)
            // One physical controller can briefly be reported by more than one discovery
            // source with a stale/temporary ID. Prefer the network endpoint as the identity
            // when it is available so the UI does not show duplicate cards for one ESP32.
            .groupBy { device ->
                device.baseUrl.trim().trimEnd('/').lowercase().takeIf { it.isNotBlank() }
                    ?: device.deviceId.trim().lowercase()
            }
            .mapNotNull { (_, candidates) ->
                candidates.maxWithOrNull(
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
            .sortedBy { it.deviceId }
    }.stateIn(scope, SharingStarted.Eagerly, emptyList())

    fun start() {
        nsd.start()
        udp.start()
    }

    fun stop() {
        nsd.stop()
        udp.stop()
    }

    suspend fun rescan() = coroutineScope {
        awaitAll(
            async { udp.scanOnce() },
            async { http.scanOnce() },
        )
        Unit
    }
}
