package ua.homeguard.s3.network

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.flow
import kotlinx.coroutines.flow.map
import kotlinx.coroutines.flow.stateIn
import ua.homeguard.s3.model.DiscoveredDevice
import java.util.concurrent.atomic.AtomicBoolean

internal const val DISCOVERY_FRESHNESS_MS = 30_000L
private const val DISCOVERY_FRESHNESS_TICK_MS = 1_000L

internal fun freshDiscoveryReports(
    reports: List<DiscoveredDevice>,
    nowMs: Long,
    maxAgeMs: Long = DISCOVERY_FRESHNESS_MS,
): List<DiscoveredDevice> {
    val cutoff = nowMs - maxAgeMs.coerceAtLeast(0L)
    return reports.filter { it.seenAtMs >= cutoff }
}

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
    private val manualRescanRunning = AtomicBoolean(false)
    private val freshnessClock = flow {
        while (true) {
            emit(System.currentTimeMillis())
            delay(DISCOVERY_FRESHNESS_TICK_MS)
        }
    }

    val scanStatus: StateFlow<UdpDeviceDiscovery.ScanStatus> = combine(
        udp.status,
        http.status,
        manualRescanActive,
        ::combineDiscoveryScanStatus,
    ).stateIn(scope, SharingStarted.Eagerly, UdpDeviceDiscovery.ScanStatus())

    val isScanning: StateFlow<Boolean> = scanStatus
        .map { it.phase == "sending" || it.phase == "listening" || it.phase == "scanning" }
        .stateIn(scope, SharingStarted.Eagerly, false)

    val devices: StateFlow<List<DiscoveredDevice>> = combine(
        nsd.devices,
        udp.devices,
        http.devices,
        freshnessClock,
    ) { mdns, udpFallback, httpFallback, nowMs ->
        val fresh = freshDiscoveryReports(
            mdns + udpFallback + httpFallback,
            nowMs,
        )
        DiscoveryDeduplicator.collapse(fresh)
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
        // A rapid double tap can call rescan() twice before Compose disables the button.
        // Do not queue another full UDP+HTTP pass behind the first one: that causes
        // duplicate network work and can make scan state briefly report idle/done while
        // another scan is still executing.
        if (!manualRescanRunning.compareAndSet(false, true)) return

        manualRescanActive.value = true
        try {
            coroutineScope {
                awaitAll(
                    async { udp.scanOnce() },
                    async { http.scanOnce() },
                )
            }
        } finally {
            manualRescanActive.value = false
            manualRescanRunning.set(false)
        }
    }
}
