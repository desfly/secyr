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

    suspend fun rescan() {
        if (manualScanActive.value) return
        manualScanActive.value = true
        try {
            // Manual discovery is an explicit request from the operator. First run the
            // lightweight UDP discovery, then verify the LAN with the bounded HTTP scan.
            // This is important for "add by device ID": that record has no IP/baseUrl yet,
            // so it must not stay offline merely because UDP/mDNS did not resolve the ID.
            // HTTP is still serialized after UDP and limited by HttpSubnetDiscovery, so we
            // do not recreate the earlier burst of simultaneous discovery traffic.
            http.clear()
            udp.scanOnce()
            http.scanOnce()
        } finally {
            manualScanActive.value = false
        }
    }
}
