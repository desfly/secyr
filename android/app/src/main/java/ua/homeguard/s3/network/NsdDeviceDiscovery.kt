package ua.homeguard.s3.network

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.DiscoverySource
import ua.homeguard.s3.model.Transport
import java.nio.charset.StandardCharsets
import java.util.concurrent.ConcurrentHashMap

class NsdDeviceDiscovery(context: Context) {
    companion object { const val SERVICE_TYPE = "_homeguard._tcp." }

    private val nsd = context.getSystemService(Context.NSD_SERVICE) as NsdManager
    private val found = ConcurrentHashMap<String, DiscoveredDevice>()
    private val _devices = MutableStateFlow<List<DiscoveredDevice>>(emptyList())
    val devices: StateFlow<List<DiscoveredDevice>> = _devices.asStateFlow()
    private var started = false

    private val resolveListener = object : NsdManager.ResolveListener {
        override fun onResolveFailed(serviceInfo: NsdServiceInfo, errorCode: Int) = Unit
        override fun onServiceResolved(serviceInfo: NsdServiceInfo) {
            val attributes = serviceInfo.attributes
            fun attribute(name: String): String? = attributes[name]?.toString(StandardCharsets.UTF_8)
            val deviceId = attribute("id") ?: serviceInfo.serviceName
            val host = attribute("host") ?: serviceInfo.host?.hostName ?: return
            val secure = attribute("tls") != "0"
            val apiVersion = attribute("api")?.toIntOrNull() ?: 1
            found[deviceId] = DiscoveredDevice(
                deviceId = deviceId,
                serviceName = serviceInfo.serviceName,
                host = host.substringBefore('%').trimEnd('.'),
                port = serviceInfo.port,
                secure = secure,
                apiVersion = apiVersion,
                source = DiscoverySource.MDNS
            )
            publish()
        }
    }

    private val discoveryListener = object : NsdManager.DiscoveryListener {
        override fun onDiscoveryStarted(serviceType: String) = Unit
        override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) { stop() }
        override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) = Unit
        override fun onDiscoveryStopped(serviceType: String) = Unit
        override fun onServiceFound(serviceInfo: NsdServiceInfo) {
            if (serviceInfo.serviceType.startsWith("_homeguard._tcp")) {
                @Suppress("DEPRECATION")
                nsd.resolveService(serviceInfo, resolveListener)
            }
        }
        override fun onServiceLost(serviceInfo: NsdServiceInfo) {
            val keys = found.filterValues { it.serviceName == serviceInfo.serviceName }.keys
            keys.forEach(found::remove)
            publish()
        }
    }

    @Synchronized fun start() {
        if (started) return
        started = true
        nsd.discoverServices(SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, discoveryListener)
    }

    @Synchronized fun stop() {
        if (!started) return
        started = false
        runCatching { nsd.stopServiceDiscovery(discoveryListener) }
    }

    private fun publish() {
        val expiry = System.currentTimeMillis() - 30_000L
        found.entries.removeIf { it.value.seenAtMs < expiry }
        _devices.value = found.values.sortedBy { it.deviceId }
    }
}
