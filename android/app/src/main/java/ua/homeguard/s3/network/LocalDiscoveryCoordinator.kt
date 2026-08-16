package ua.homeguard.s3.network

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import ua.homeguard.s3.model.DeviceIdentity
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.DiscoverySource

class LocalDiscoveryCoordinator(context: Context, private val scope: CoroutineScope) {
    companion object {
        // UDP is refreshed every 5 s and HTTP is scan-scoped. mDNS normally
        // supplies onServiceLost, but some Android stacks miss that callback;
        // keep a longer grace window while still preventing permanent online.
        private const val ACTIVE_DISCOVERY_TTL_MS = 30_000L
        private const val MDNS_DISCOVERY_TTL_MS = 90_000L
    }

    private val appContext = context.applicationContext
    private val nsd = NsdDeviceDiscovery(appContext)
    private val udp = UdpDeviceDiscovery(appContext, scope)
    private val http = HttpSubnetDiscovery(appContext)
    private val manualScanActive = MutableStateFlow(false)

    val scanStatus: StateFlow<UdpDeviceDiscovery.ScanStatus> = udp.status
    val isScanning: StateFlow<Boolean> = combine(
        udp.status.map { it.phase == "sending" || it.phase == "listening" },
        manualScanActive,
    ) { udpScanning, manualScanning -> udpScanning || manualScanning }
        .stateIn(scope, SharingStarted.Eagerly, false)

    val devices: StateFlow<List<DiscoveredDevice>> = combine(
        nsd.devices,
        udp.devices,
        http.devices,
        // Scan status changes every periodic UDP pass and gives this combine a
        // heartbeat even when a stale source list itself has not changed.
        udp.status,
    ) { mdns, udpFallback, httpFallback, _ ->
        val now = System.currentTimeMillis()
        val all = (mdns + udpFallback + httpFallback).filter { candidate ->
            val ttl = if (candidate.source == DiscoverySource.MDNS) {
                MDNS_DISCOVERY_TTL_MS
            } else {
                ACTIVE_DISCOVERY_TTL_MS
            }
            val age = (now - candidate.seenAtMs).coerceAtLeast(0L)
            age <= ttl
        }
        val groups = mutableListOf<MutableList<DiscoveredDevice>>()

        // Different discovery transports may spell the same controller differently
        // (service name, HG-* id, HTTP endpoint). Fold them into one physical device.
        all.sortedByDescending { it.seenAtMs }.forEach { candidate ->
            val group = groups.firstOrNull { existing ->
                existing.any { member ->
                    DeviceIdentity.samePhysicalDevice(
                        member.deviceId,
                        member.baseUrl,
                        candidate.deviceId,
                        candidate.baseUrl,
                    )
                }
            }
            if (group == null) groups += mutableListOf(candidate) else group += candidate
        }

        groups.mapNotNull { candidates ->
            val winner = candidates.maxWithOrNull(
                compareBy<DiscoveredDevice> { it.seenAtMs }
                    .thenBy {
                        when (it.source) {
                            DiscoverySource.HTTP -> 3
                            DiscoverySource.UDP -> 2
                            DiscoverySource.MDNS -> 1
                        }
                    },
            ) ?: return@mapNotNull null

            val realId = candidates
                .filter { it.deviceId.startsWith("HG-", ignoreCase = true) }
                .maxByOrNull { it.seenAtMs }
                ?.deviceId
                ?: winner.deviceId

            val cloudCandidate = candidates
                .filter { it.cloudConfigured != null || it.cloudConnected != null }
                .maxByOrNull { it.seenAtMs }

            winner.copy(
                deviceId = realId,
                cloudConfigured = cloudCandidate?.cloudConfigured ?: winner.cloudConfigured,
                cloudConnected = cloudCandidate?.cloudConnected ?: winner.cloudConnected,
            )
        }.sortedBy { DeviceIdentity.canonicalId(it.deviceId) }
    }.stateIn(scope, SharingStarted.Eagerly, emptyList())

    fun start() {
        nsd.start()
        udp.start()
    }

    fun stop() {
        nsd.stop()
        udp.stop()
        http.clear()
    }

    suspend fun rescan() {
        if (manualScanActive.value) return
        manualScanActive.value = true
        try {
            http.clear()
            udp.scanOnce()
            http.scanOnce()
        } finally {
            manualScanActive.value = false
        }
    }
}
