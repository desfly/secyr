package ua.homeguard.s3.network

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.util.Log
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.DiscoverySource
import java.nio.charset.StandardCharsets
import java.util.concurrent.ConcurrentHashMap

internal fun isDiscoveryGenerationActive(started: Boolean, activeGeneration: Long, callbackGeneration: Long): Boolean =
    started && activeGeneration == callbackGeneration

class NsdDeviceDiscovery(context: Context) {
    companion object {
        const val SERVICE_TYPE = "_homeguard._tcp."
        private const val TAG = "HomeGuardMDNS"
    }

    private val nsd = context.getSystemService(Context.NSD_SERVICE) as NsdManager
    private val found = ConcurrentHashMap<String, DiscoveredDevice>()
    private val _devices = MutableStateFlow<List<DiscoveredDevice>>(emptyList())
    val devices: StateFlow<List<DiscoveredDevice>> = _devices.asStateFlow()
    @Volatile private var started = false
    @Volatile private var generation = 0L
    @Volatile private var activeDiscoveryListener: NsdManager.DiscoveryListener? = null

    private fun isActive(callbackGeneration: Long): Boolean =
        isDiscoveryGenerationActive(started, generation, callbackGeneration)

    private fun resolve(serviceInfo: NsdServiceInfo, resolveGeneration: Long) {
        val listener = object : NsdManager.ResolveListener {
            override fun onResolveFailed(info: NsdServiceInfo, errorCode: Int) {
                if (!isActive(resolveGeneration)) return
                Log.w(TAG, "mDNS resolve failed: service=${info.serviceName} code=$errorCode")
            }

            override fun onServiceResolved(info: NsdServiceInfo) {
                if (!isActive(resolveGeneration)) return

                val attributes = info.attributes
                fun attribute(name: String): String? = attributes[name]?.toString(StandardCharsets.UTF_8)

                val deviceId = attribute("id") ?: info.serviceName
                // Prefer the concrete address resolved by Android. TXT host metadata can
                // contain a *.local name which OkHttp may not be able to resolve later.
                val host = info.host?.hostAddress
                    ?: attribute("host")
                    ?: info.host?.hostName
                    ?: run {
                        Log.w(TAG, "mDNS resolve returned no usable host: service=${info.serviceName}")
                        return
                    }
                val secure = attribute("tls") != "0"
                val apiVersion = attribute("api")?.toIntOrNull() ?: 1

                found[deviceId] = DiscoveredDevice(
                    deviceId = deviceId,
                    serviceName = info.serviceName,
                    host = host.substringBefore('%').trimEnd('.'),
                    port = info.port,
                    secure = secure,
                    apiVersion = apiVersion,
                    source = DiscoverySource.MDNS,
                )
                Log.i(TAG, "HomeGuard mDNS found: id=$deviceId host=$host port=${info.port}")
                publish()
            }
        }

        @Suppress("DEPRECATION")
        nsd.resolveService(serviceInfo, listener)
    }

    private fun discoveryListener(runGeneration: Long): NsdManager.DiscoveryListener =
        object : NsdManager.DiscoveryListener {
            override fun onDiscoveryStarted(serviceType: String) {
                if (!isActive(runGeneration)) return
                Log.d(TAG, "mDNS discovery started: $serviceType")
            }

            override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) {
                if (!isActive(runGeneration)) return
                Log.w(TAG, "mDNS discovery start failed: type=$serviceType code=$errorCode")
                failRun(runGeneration, this)
            }

            override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) {
                Log.w(TAG, "mDNS discovery stop failed: type=$serviceType code=$errorCode")
            }

            override fun onDiscoveryStopped(serviceType: String) {
                Log.d(TAG, "mDNS discovery stopped: $serviceType")
            }

            override fun onServiceFound(serviceInfo: NsdServiceInfo) {
                if (!isActive(runGeneration)) return
                if (serviceInfo.serviceType.startsWith("_homeguard._tcp")) {
                    Log.d(TAG, "mDNS service announced: ${serviceInfo.serviceName}")
                    resolve(serviceInfo, runGeneration)
                }
            }

            override fun onServiceLost(serviceInfo: NsdServiceInfo) {
                // The platform can deliver a callback from the previous discovery run
                // after stop()->start(). Bind lost/found callbacks to their own run so an
                // old lost event cannot delete a freshly resolved endpoint.
                if (!isActive(runGeneration)) return
                val keys = found.filterValues { it.serviceName == serviceInfo.serviceName }.keys
                keys.forEach(found::remove)
                if (keys.isNotEmpty()) {
                    Log.d(TAG, "mDNS service lost: ${serviceInfo.serviceName}")
                }
                publish()
            }
        }

    @Synchronized
    private fun failRun(runGeneration: Long, listener: NsdManager.DiscoveryListener) {
        if (!isDiscoveryGenerationActive(started, generation, runGeneration)) return
        if (activeDiscoveryListener !== listener) return
        started = false
        generation += 1
        activeDiscoveryListener = null
        runCatching { nsd.stopServiceDiscovery(listener) }
            .onFailure { Log.w(TAG, "Unable to clean up failed mDNS discovery", it) }
    }

    @Synchronized
    fun start() {
        if (started) return
        generation += 1
        val runGeneration = generation
        val listener = discoveryListener(runGeneration)
        activeDiscoveryListener = listener
        started = true
        runCatching {
            nsd.discoverServices(SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, listener)
        }.onFailure { error ->
            if (generation == runGeneration && activeDiscoveryListener === listener) {
                started = false
                generation += 1
                activeDiscoveryListener = null
            }
            Log.w(TAG, "Unable to start mDNS discovery", error)
        }
    }

    @Synchronized
    fun stop() {
        if (!started) return
        val listener = activeDiscoveryListener
        started = false
        generation += 1
        activeDiscoveryListener = null
        if (listener != null) {
            runCatching { nsd.stopServiceDiscovery(listener) }
                .onFailure { Log.w(TAG, "Unable to stop mDNS discovery", it) }
        }
    }

    private fun publish() {
        val expiry = System.currentTimeMillis() - 30_000L
        found.entries.removeIf { it.value.seenAtMs < expiry }
        _devices.value = found.values.sortedBy { it.deviceId }
    }
}
