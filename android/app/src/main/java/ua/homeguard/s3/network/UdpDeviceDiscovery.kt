package ua.homeguard.s3.network

import android.util.Log
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
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

class UdpDeviceDiscovery(private val scope: CoroutineScope) {
    data class ScanStatus(
        val phase: String = "idle",
        val progress: Float = 0f,
        val targets: List<String> = emptyList(),
        val sent: Int = 0,
        val received: Int = 0,
        val accepted: Int = 0,
        val lastResponder: String = "",
        val error: String = "",
    )

    companion object {
        const val PORT = 45678
        const val REQUEST = "HG_DISCOVER_V1"
        private const val TAG = "HomeGuardDiscovery"
    }

    private val found = ConcurrentHashMap<String, DiscoveredDevice>()
    private val _devices = MutableStateFlow<List<DiscoveredDevice>>(emptyList())
    val devices: StateFlow<List<DiscoveredDevice>> = _devices.asStateFlow()
    private val _status = MutableStateFlow(ScanStatus())
    val status: StateFlow<ScanStatus> = _status.asStateFlow()
    private var job: Job? = null

    fun start() {
        if (job != null) return
        job = scope.launch(Dispatchers.IO) {
            while (isActive) {
                runCatching { scanOnce() }
                    .onFailure { error ->
                        Log.w(TAG, "UDP discovery scan failed", error)
                        _status.value = _status.value.copy(
                            phase = "error",
                            progress = 1f,
                            error = error.message ?: error.javaClass.simpleName,
                        )
                    }
                delay(5_000L)
            }
        }
    }

    fun stop() {
        job?.cancel()
        job = null
    }

    suspend fun scanOnce(timeoutMs: Int = 2_400) = withContext(Dispatchers.IO) {
        val targets = broadcastAddresses().mapNotNull { it.hostAddress }.distinct()
        _status.value = ScanStatus(phase = "sending", progress = 0.05f, targets = targets)

        DatagramSocket().use { socket ->
            socket.broadcast = true
            socket.soTimeout = 120
            val request = REQUEST.toByteArray(Charsets.UTF_8)
            var sent = 0
            var received = 0
            var accepted = 0

            targets.forEachIndexed { index, rawAddress ->
                val address = InetAddress.getByName(rawAddress)
                runCatching {
                    socket.send(DatagramPacket(request, request.size, address, PORT))
                    sent++
                    Log.d(TAG, "Discovery request sent to ${address.hostAddress}:$PORT")
                }.onFailure { error ->
                    Log.w(TAG, "Discovery send failed for ${address.hostAddress}:$PORT", error)
                }
                val sendProgress = 0.05f + 0.15f * ((index + 1).toFloat() / targets.size.coerceAtLeast(1))
                _status.value = _status.value.copy(progress = sendProgress, sent = sent)
            }

            _status.value = _status.value.copy(phase = "listening", progress = 0.20f, sent = sent)
            Log.d(TAG, "UDP discovery listening: sent=$sent targets=${targets.size}")

            val startedAt = System.currentTimeMillis()
            val deadline = startedAt + timeoutMs
            val buffer = ByteArray(1024)
            while (System.currentTimeMillis() < deadline) {
                val elapsed = System.currentTimeMillis() - startedAt
                val listenFraction = (elapsed.toFloat() / timeoutMs.coerceAtLeast(1)).coerceIn(0f, 1f)
                _status.value = _status.value.copy(progress = 0.20f + 0.75f * listenFraction)

                val packet = DatagramPacket(buffer, buffer.size)
                try {
                    socket.receive(packet)
                    received++
                    val responder = packet.address.hostAddress?.substringBefore('%').orEmpty()
                    val parsed = runCatching { parse(packet) }
                        .onFailure { error -> Log.w(TAG, "Invalid discovery reply from $responder", error) }
                        .getOrNull()
                    if (parsed != null) {
                        accepted++
                        found[parsed.deviceId] = parsed
                        Log.i(TAG, "HomeGuard found: id=${parsed.deviceId} host=${parsed.host} port=${parsed.port}")
                    }
                    _status.value = _status.value.copy(
                        received = received,
                        accepted = accepted,
                        lastResponder = responder,
                    )
                } catch (_: SocketTimeoutException) {
                    // Timeout is expected; loop updates progress until the scan window closes.
                }
            }

            Log.d(TAG, "UDP discovery complete: received=$received accepted=$accepted devices=${found.size}")
            _status.value = _status.value.copy(
                phase = "done",
                progress = 1f,
                sent = sent,
                received = received,
                accepted = accepted,
            )
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
            _status.value = _status.value.copy(error = error.message ?: error.javaClass.simpleName)
        }
        addresses += InetAddress.getByName("255.255.255.255")
        return addresses
    }
}
