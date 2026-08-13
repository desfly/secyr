package ua.homeguard.s3.network

import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONObject
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.DiscoverySource
import ua.homeguard.s3.model.Transport
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.Inet4Address
import java.net.InetAddress
import java.net.NetworkInterface
import java.net.SocketTimeoutException
import java.util.Collections
import java.util.concurrent.ConcurrentHashMap
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

class UdpDeviceDiscovery(private val scope: CoroutineScope) {
    companion object {
        const val PORT = 45678
        const val REQUEST = "HG_DISCOVER_V1"
        private const val TAG = "HomeGuardDiscovery"
    }

    private val found = ConcurrentHashMap<String, DiscoveredDevice>()
    private val _devices = MutableStateFlow<List<DiscoveredDevice>>(emptyList())
    val devices: StateFlow<List<DiscoveredDevice>> = _devices.asStateFlow()
    private var job: Job? = null

    fun start() {
        if (job != null) return
        job = scope.launch(Dispatchers.IO) {
            while (isActive) {
                runCatching { scanOnce() }
                    .onFailure { error -> Log.w(TAG, "UDP discovery scan failed", error) }
                delay(5_000L)
            }
        }
    }

    fun stop() {
        job?.cancel()
        job = null
    }

    suspend fun scanOnce(timeoutMs: Int = 1_200) = withContext(Dispatchers.IO) {
        DatagramSocket().use { socket ->
            socket.broadcast = true
            socket.soTimeout = 200
            val request = REQUEST.toByteArray(Charsets.UTF_8)
            val targets = broadcastAddresses()
            var sent = 0
            targets.forEach { address ->
                runCatching {
                    socket.send(DatagramPacket(request, request.size, address, PORT))
                    sent++
                    Log.d(TAG, "Discovery request sent to ${address.hostAddress}:$PORT")
                }.onFailure { error ->
                    Log.w(TAG, "Discovery send failed for ${address.hostAddress}:$PORT", error)
                }
            }
            Log.d(TAG, "UDP discovery listening: sent=$sent targets=${targets.size}")

            val deadline = System.currentTimeMillis() + timeoutMs
            val buffer = ByteArray(1024)
            var received = 0
            while (System.currentTimeMillis() < deadline) {
                val packet = DatagramPacket(buffer, buffer.size)
                try {
                    socket.receive(packet)
                    received++
                    parse(packet)?.let { device ->
                        found[device.deviceId] = device
                        Log.i(TAG, "HomeGuard found: id=${device.deviceId} host=${device.host} port=${device.port}")
                    }
                } catch (_: SocketTimeoutException) {
                    // Continue until the bounded scan window closes.
                }
            }
            Log.d(TAG, "UDP discovery complete: received=$received devices=${found.size}")
        }

        val expiry = System.currentTimeMillis() - 30_000L
        found.entries.removeIf { it.value.seenAtMs < expiry }
        _devices.value = found.values.sortedBy { it.deviceId }
    }

    private fun parse(packet: DatagramPacket): DiscoveredDevice? {
        val json = JSONObject(String(packet.data, packet.offset, packet.length, Charsets.UTF_8))
        if (json.optString("protocol") != "homeguard-discovery-v1") return null
        val deviceId = json.optString("device_id")
        if (deviceId.isBlank()) return null
        val responderIp = packet.address.hostAddress?.substringBefore('%') ?: return null
        val transport = runCatching {
            Transport.valueOf(json.optString("transport", "none").uppercase())
        }.getOrDefault(Transport.NONE)

        return DiscoveredDevice(
            deviceId = deviceId,
            serviceName = json.optString("hostname", deviceId),
            host = responderIp,
            port = json.optInt("port", 443),
            secure = json.optBoolean("secure", true),
            apiVersion = json.optInt("api_version", 1),
            transport = transport,
            pairingRequired = json.optBoolean("pairing_required", false),
            source = DiscoverySource.UDP
        )
    }

    private fun broadcastAddresses(): Set<InetAddress> {
        val addresses = linkedSetOf<InetAddress>()
        runCatching {
            Collections.list(NetworkInterface.getNetworkInterfaces())
                .filter { it.isUp && !it.isLoopback }
                .flatMap { it.interfaceAddresses }
                .mapNotNull { it.broadcast }
                .filterIsInstance<Inet4Address>()
                .forEach(addresses::add)
        }.onFailure { error ->
            Log.w(TAG, "Unable to enumerate LAN broadcast addresses", error)
        }
        addresses += InetAddress.getByName("255.255.255.255")
        return addresses
    }
}
