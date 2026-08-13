package ua.homeguard.s3.network

import android.content.Context
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharingStarted
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.stateIn
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.DiscoverySource

class LocalDiscoveryCoordinator(context: Context, private val scope: CoroutineScope) {
    private val nsd = NsdDeviceDiscovery(context.applicationContext)
    private val udp = UdpDeviceDiscovery(scope)
    private val _isScanning = MutableStateFlow(false)

    val isScanning: StateFlow<Boolean> = _isScanning.asStateFlow()

    val devices: StateFlow<List<DiscoveredDevice>> = combine(nsd.devices, udp.devices) { mdns, fallback ->
        (mdns + fallback)
            .groupBy { it.deviceId }
            .mapNotNull { (_, candidates) ->
                candidates.maxWithOrNull(compareBy<DiscoveredDevice> {
                    if (it.source == DiscoverySource.MDNS) 1 else 0
                }.thenBy { it.seenAtMs })
            }
            .sortedBy { it.deviceId }
    }.stateIn(scope, SharingStarted.Eagerly, emptyList())

    fun start() { nsd.start(); udp.start() }
    fun stop() { nsd.stop(); udp.stop() }

    suspend fun rescan() {
        if (_isScanning.value) return
        _isScanning.value = true
        try {
            // NSD runs continuously after start(); this forces the UDP fallback
            // to probe the current LAN immediately instead of showing an animation.
            udp.scanOnce()
        } finally {
            _isScanning.value = false
        }
    }
}
