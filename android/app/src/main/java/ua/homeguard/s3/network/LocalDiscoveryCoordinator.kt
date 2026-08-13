package ua.homeguard.s3.network

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.DiscoverySource

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
        nsd.start()
        udp.scanOnce()
    }
}
