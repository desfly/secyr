package ua.homeguard.s3.network

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.DiscoverySource

data class LocalDiscoveryProgressState(
    val running: Boolean = false,
    val fraction: Float = 0f,
    val phase: String = "",
)

object LocalDiscoveryProgress {
    private val _state = MutableStateFlow(LocalDiscoveryProgressState())
    val state: StateFlow<LocalDiscoveryProgressState> = _state.asStateFlow()

    fun begin() {
        _state.value = LocalDiscoveryProgressState(
            running = true,
            fraction = 0f,
            phase = "Підготовка пошуку",
        )
    }

    fun mdnsStarted() {
        _state.value = LocalDiscoveryProgressState(
            running = true,
            fraction = 0.2f,
            phase = "mDNS: пошук сервісу",
        )
    }

    fun udpStarted() {
        _state.value = LocalDiscoveryProgressState(
            running = true,
            fraction = 0.35f,
            phase = "UDP: очікування відповідей",
        )
    }

    fun finish() {
        _state.value = LocalDiscoveryProgressState(
            running = false,
            fraction = 1f,
            phase = "Пошук завершено",
        )
    }
}

class LocalDiscoveryCoordinator(context: Context, private val scope: CoroutineScope) {
    private val nsd = NsdDeviceDiscovery(context.applicationContext)
    private val udp = UdpDeviceDiscovery(scope)
    private var retryJob: Job? = null

    val devices: StateFlow<List<DiscoveredDevice>> = combine(nsd.devices, udp.devices) { mdns, fallback ->
        (mdns + fallback)
            .groupBy { it.deviceId }
            .mapNotNull { (_, candidates) ->
                candidates.maxWithOrNull(
                    compareBy<DiscoveredDevice> {
                        if (it.source == DiscoverySource.MDNS) 1 else 0
                    }.thenBy { it.seenAtMs },
                )
            }
            .sortedBy { it.deviceId }
    }.stateIn(scope, SharingStarted.Eagerly, emptyList())

    fun start() {
        nsd.start()
        udp.start()
        if (retryJob == null) {
            retryJob = scope.launch {
                while (isActive) {
                    delay(5_000L)
                    nsd.start()
                }
            }
        }
    }

    fun stop() {
        retryJob?.cancel()
        retryJob = null
        nsd.stop()
        udp.stop()
    }

    suspend fun rescan() {
        LocalDiscoveryProgress.begin()
        try {
            nsd.start()
            LocalDiscoveryProgress.mdnsStarted()
            LocalDiscoveryProgress.udpStarted()
            udp.scanOnce()
        } finally {
            LocalDiscoveryProgress.finish()
        }
    }
}
