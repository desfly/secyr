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
            .mapNotNull { candidates ->
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

    /**
     * Cemented rule: one physical ESP controller is one UI device.
     *
     * A stable controller ID survives DHCP address changes, while the LAN host
     * bridges discovery paths that only have a temporary setup ID. Identity is
     * therefore an equivalence relation: candidates belong together when they
     * share a non-temporary deviceId OR the same concrete host. The connected-
     * component expansion below also handles a bridge such as stable-ID@old-IP,
     * stable-ID@new-IP and setup-ID@new-IP without producing two cards.
     */
    private fun mergePhysicalControllers(devices: List<DiscoveredDevice>): List<List<DiscoveredDevice>> {
        val remaining = devices.toMutableList()
        val groups = mutableListOf<List<DiscoveredDevice>>()

        while (remaining.isNotEmpty()) {
            val group = mutableListOf(remaining.removeAt(remaining.lastIndex))
            var expanded: Boolean
            do {
                expanded = false
                val iterator = remaining.iterator()
                while (iterator.hasNext()) {
                    val candidate = iterator.next()
                    if (group.any { samePhysicalController(it, candidate) }) {
                        group += candidate
                        iterator.remove()
                        expanded = true
                    }
                }
            } while (expanded)
            groups += group
        }
        return groups
    }

    private fun samePhysicalController(a: DiscoveredDevice, b: DiscoveredDevice): Boolean {
        val aHost = normalizedHost(a)
        val bHost = normalizedHost(b)
        if (aHost.isNotBlank() && aHost == bHost) return true

        val aId = stableDeviceId(a)
        val bId = stableDeviceId(b)
        return aId != null && aId == bId
    }

    private fun normalizedHost(device: DiscoveredDevice): String =
        device.host.trim().trim('[', ']').substringBefore('%').trimEnd('.').lowercase()

    private fun stableDeviceId(device: DiscoveredDevice): String? {
        val id = device.deviceId.trim().lowercase()
        if (id.isBlank() || id.startsWith("setup-")) return null
        return id
    }
}
