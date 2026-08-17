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
        mergePhysicalControllers(mdns + udpFallback + httpFallback)
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

    /**
     * Cemented rule: one physical ESP controller is one discovery card.
     *
     * A simple groupBy(host) is not sufficient: mDNS may return a hostname while
     * UDP/HTTP return an IP address, and DHCP can change that IP later. Conversely,
     * two discovery transports can temporarily expose different IDs for the same
     * LAN endpoint. We therefore merge transitively when either the stable device ID
     * or the normalized endpoint identifies the same controller.
     */
    private fun mergePhysicalControllers(input: List<DiscoveredDevice>): List<DiscoveredDevice> {
        val clusters = mutableListOf<MutableList<DiscoveredDevice>>()

        input.forEach { candidate ->
            val matching = clusters.withIndex().filter { (_, cluster) ->
                cluster.any { existing ->
                    ControllerIdentity.sameController(
                        existing.deviceId,
                        existing.baseUrl,
                        candidate.deviceId,
                        candidate.baseUrl,
                    )
                }
            }

            if (matching.isEmpty()) {
                clusters += mutableListOf(candidate)
            } else {
                val targetIndex = matching.first().index
                clusters[targetIndex] += candidate
                matching.drop(1).asReversed().forEach { indexed ->
                    clusters[targetIndex] += clusters[indexed.index]
                    clusters.removeAt(indexed.index)
                }
            }
        }

        return clusters.mapNotNull { candidates ->
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
    }
}
