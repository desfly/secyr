package ua.homeguard.s3.network

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import ua.homeguard.s3.model.DiscoveredDevice

internal fun combineDiscoveryScanStatus(
    udp: UdpDeviceDiscovery.ScanStatus,
    http: HttpSubnetDiscovery.ScanStatus,
    manualRescanActive: Boolean,
): UdpDeviceDiscovery.ScanStatus {
    val udpActive = udp.phase == "sending" || udp.phase == "listening"
    val hasError = udp.phase == "error" || http.phase == "error"
    val error = listOf(udp.error, http.error)
        .filter { it.isNotBlank() }
        .distinct()
        .joinToString(" · ")

    val phase = when {
        manualRescanActive -> "scanning"
        udpActive -> udp.phase
        hasError -> "error"
        udp.phase == "done" || http.phase == "done" -> "done"
        else -> udp.phase
    }

    val progress = when {
        manualRescanActive -> ((udp.progress.coerceIn(0f, 1f) + http.progress.coerceIn(0f, 1f)) / 2f)
            .coerceAtMost(0.99f)
        phase == "done" || phase == "error" -> 1f
        else -> udp.progress.coerceIn(0f, 1f)
    }

    return udp.copy(
        phase = phase,
        progress = progress,
        accepted = udp.accepted + http.found,
        error = error,
    )
}

class LocalDiscoveryCoordinator(context: Context, private val scope: CoroutineScope) {
    private val appContext = context.applicationContext
    private val nsd = NsdDeviceDiscovery(appContext)
    private val udp = UdpDeviceDiscovery(appContext, scope)
    private val http = HttpSubnetDiscovery(appContext)
    private val manualRescanActive = MutableStateFlow(false)

    val scanStatus: StateFlow<UdpDeviceDiscovery.ScanStatus> = combine(
        udp.status,
        http.status,
        manualRescanActive,
        ::combineDiscoveryScanStatus,
    ).stateIn(scope, SharingStarted.Eagerly, UdpDeviceDiscovery.ScanStatus())

    val isScanning: StateFlow<Boolean> = scanStatus
        .map { it.phase == "sending" || it.phase == "listening" || it.phase == "scanning" }
        .stateIn(scope, SharingStarted.Eagerly, false)

    val devices: StateFlow<List<DiscoveredDevice>> = combine(nsd.devices, udp.devices, http.devices) { mdns, udpFallback, httpFallback ->
        // Cemented rule: one physical ESP controller is one UI device.
        // Use transitive identity reconciliation instead of grouping by host only:
        // mDNS/UDP/HTTP may observe the same controller through different hosts or IDs.
        DiscoveryDeduplicator.collapse(mdns + udpFallback + httpFallback)
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
        manualRescanActive.value = true
        try {
            awaitAll(
                async { udp.scanOnce() },
                async { http.scanOnce() },
            )
        } finally {
            manualRescanActive.value = false
        }
        Unit
    }
}
