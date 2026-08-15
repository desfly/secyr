package ua.homeguard.s3.network

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.MutableStateFlow
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
    private val manualScanActive = MutableStateFlow(false)

    val scanStatus: StateFlow<UdpDeviceDiscovery.ScanStatus> = udp.status
    val isScanning: StateFlow<Boolean> = combine(
        udp.status.map { it.phase == "sending" || it.phase == "listening" },
        manualScanActive,
    ) { udpScanning, manualScanning ->
        udpScanning || manualScanning
    }.stateIn(scope, SharingStarted.Eagerly, false)

    val devices: StateFlow<List<DiscoveredDevice>> = combine(nsd.devices, udp.devices, http.devices) { mdns, udpFallback, httpFallback ->
        (mdns + udpFallback + httpFallback)
            .groupBy { device ->
                val id = device.deviceId.trim()
                if (id.startsWith("HG-", ignoreCase = true)) {
                    "id:${id.lowercase()}"
                } else {
                    "net:${device.baseUrl.trim().trimEnd('/').lowercase()}"
                }
            }
            .mapNotNull { (_, candidates) ->
                val winner = candidates.maxWithOrNull(
                    compareBy<DiscoveredDevice> { it.seenAtMs }
                        .thenBy {
                            when (it.source) {
                                DiscoverySource.MDNS -> 2
                                DiscoverySource.UDP -> 1
                                DiscoverySource.HTTP -> 0
                            }
                        },
                ) ?: return@mapNotNull null

                // HTTP discovery reads /api/v1/cloud/status and therefore knows the real
                // CLOUD state. Keep that status even when a fresher mDNS/UDP candidate wins
                // the endpoint selection for the same physical controller.
                val cloudCandidate = candidates
                    .filter { it.cloudConfigured != null || it.cloudConnected != null }
                    .maxByOrNull { it.seenAtMs }

                winner.copy(
                    cloudConfigured = cloudCandidate?.cloudConfigured ?: winner.cloudConfigured,
                    cloudConnected = cloudCandidate?.cloudConnected ?: winner.cloudConnected,
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

    suspend fun rescan() {
        if (manualScanActive.value) return
        manualScanActive.value = true
        try {
            // Manual discovery is explicit: lightweight UDP first, then bounded HTTP.
            // HTTP verifies the real device ID and also refreshes the per-device CLOUD state.
            http.clear()
            udp.scanOnce()
            http.scanOnce()
        } finally {
            manualScanActive.value = false
        }
    }
}
