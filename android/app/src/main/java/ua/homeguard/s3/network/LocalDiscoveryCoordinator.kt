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
    private val appContext = context.applicationContext
    private val nsd = NsdDeviceDiscovery(appContext)
    private val udp = UdpDeviceDiscovery(appContext, scope)
    private val _isScanning = MutableStateFlow(false)

    val isScanning: StateFlow<Boolean> = _isScanning.asStateFlow()
    val scanStatus: StateFlow<UdpDeviceDiscovery.ScanStatus> = udp.status

    val devices: StateFlow<List<DiscoveredDevice>> = combine(nsd.devices, udp.devices) { mdns, fallback ->
        (mdns + fallback)
            .groupBy { it.deviceId }
            .mapNotNull { (_, candidates) ->
                // Prefer the freshest endpoint first. mDNS remains the tie-breaker only
                // when both discovery paths observed the same device at effectively the
                // same time. This prevents an older cached mDNS address from masking a
                // newer UDP response after Wi-Fi roam, DHCP change, or controller reboot.
                candidates.maxWithOrNull(compareBy<DiscoveredDevice> { it.seenAtMs }
                    .thenBy { if (it.source == DiscoverySource.MDNS) 1 else 0 })
            }
            .sortedBy { it.deviceId }
    }.stateIn(scope, SharingStarted.Eagerly, emptyList())

    fun start() { nsd.start(); udp.start() }
    fun stop() { nsd.stop(); udp.stop() }

    suspend fun rescan() {
        if (_isScanning.value) return
        _isScanning.value = true
        try {
            udp.scanOnce()
        } finally {
            _isScanning.value = false
        }
    }
}
