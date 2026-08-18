package ua.homeguard.s3.network

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.async
import kotlinx.coroutines.awaitAll
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.sync.Semaphore
import kotlinx.coroutines.sync.withPermit
import kotlinx.coroutines.withContext
import org.json.JSONObject
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.DiscoverySource
import ua.homeguard.s3.model.Transport
import java.io.Reader
import java.net.HttpURLConnection
import java.net.Inet4Address
import java.net.URL
import java.util.concurrent.atomic.AtomicInteger

internal const val HTTP_DISCOVERY_MAX_RESPONSE_CHARS = 64 * 1024

internal fun readBoundedDiscoveryText(
    reader: Reader,
    maxChars: Int = HTTP_DISCOVERY_MAX_RESPONSE_CHARS,
): String? {
    val limit = maxChars.coerceAtLeast(0)
    val result = StringBuilder(minOf(limit, 4_096))
    val buffer = CharArray(2_048)
    var total = 0
    while (true) {
        val count = reader.read(buffer)
        if (count < 0) break
        total += count
        if (total > limit) return null
        result.append(buffer, 0, count)
    }
    return result.toString()
}

class HttpSubnetDiscovery(context: Context) {
    data class ScanStatus(
        val phase: String = "idle",
        val progress: Float = 0f,
        val total: Int = 0,
        val completed: Int = 0,
        val found: Int = 0,
        val error: String = "",
    )

    companion object {
        private const val TAG = "HomeGuardHttpScan"
        private const val CONNECT_TIMEOUT_MS = 220
        private const val READ_TIMEOUT_MS = 320
        private const val SETUP_CONNECT_TIMEOUT_MS = 1_200
        private const val SETUP_READ_TIMEOUT_MS = 1_500
        private const val MAX_PARALLEL = 32
        private const val SETUP_PREFIX = "192.168.4"
        private const val SETUP_CONTROLLER_IP = "192.168.4.1"
    }

    private val connectivity = context.applicationContext.getSystemService(ConnectivityManager::class.java)
    private val _devices = MutableStateFlow<List<DiscoveredDevice>>(emptyList())
    val devices: StateFlow<List<DiscoveredDevice>> = _devices.asStateFlow()
    private val _status = MutableStateFlow(ScanStatus())
    val status: StateFlow<ScanStatus> = _status.asStateFlow()

    suspend fun scanOnce() = withContext(Dispatchers.IO) {
        _status.value = ScanStatus(phase = "preparing", progress = 0f)

        val network = findWifiNetwork()
        if (network == null) {
            _devices.value = emptyList()
            _status.value = ScanStatus(
                phase = "error",
                progress = 1f,
                error = "Wi-Fi network unavailable",
            )
            return@withContext
        }

        val local = connectivity.getLinkProperties(network)
            ?.linkAddresses
            ?.map { it.address }
            ?.filterIsInstance<Inet4Address>()
            ?.firstOrNull { !it.isLoopbackAddress && !it.isLinkLocalAddress }
        if (local == null) {
            _devices.value = emptyList()
            _status.value = ScanStatus(
                phase = "error",
                progress = 1f,
                error = "Wi-Fi IPv4 address unavailable",
            )
            return@withContext
        }

        val bytes = local.address
        val prefix = "${bytes[0].toInt() and 0xff}.${bytes[1].toInt() and 0xff}.${bytes[2].toInt() and 0xff}"
        val ownHost = bytes[3].toInt() and 0xff
        val probeSetupController = prefix == SETUP_PREFIX && ownHost != 1
        val hosts = (1..254)
            .filter { it != ownHost }
            .filterNot { prefix == SETUP_PREFIX && it == 1 }
        val total = hosts.size + if (probeSetupController) 1 else 0
        val completed = AtomicInteger(0)
        val foundCount = AtomicInteger(0)
        val progressLock = Any()

        fun record(result: DiscoveredDevice?): DiscoveredDevice? = synchronized(progressLock) {
            val done = completed.incrementAndGet()
            val foundNow = if (result != null) foundCount.incrementAndGet() else foundCount.get()
            _status.value = ScanStatus(
                phase = "probing",
                progress = if (total == 0) 1f else done.toFloat() / total.toFloat(),
                total = total,
                completed = done,
                found = foundNow,
            )
            result
        }

        _status.value = ScanStatus(phase = "probing", progress = 0f, total = total)

        val setupDevice = if (probeSetupController) {
            record(
                probe(
                    network = network,
                    ip = SETUP_CONTROLLER_IP,
                    connectTimeoutMs = SETUP_CONNECT_TIMEOUT_MS,
                    readTimeoutMs = SETUP_READ_TIMEOUT_MS,
                    allowNetworkStatusFallback = true,
                ),
            )
        } else {
            null
        }

        val semaphore = Semaphore(MAX_PARALLEL)
        val found = coroutineScope {
            hosts.map { host ->
                async(Dispatchers.IO) {
                    semaphore.withPermit {
                        record(
                            probe(
                                network = network,
                                ip = "$prefix.$host",
                                connectTimeoutMs = CONNECT_TIMEOUT_MS,
                                readTimeoutMs = READ_TIMEOUT_MS,
                                allowNetworkStatusFallback = true,
                            ),
                        )
                    }
                }
            }
                .awaitAll()
                .filterNotNull()
        }

        val devices = (listOfNotNull(setupDevice) + found)
            .associateBy { it.baseUrl.trimEnd('/').lowercase() }
            .values
            .sortedBy { it.deviceId }
        _devices.value = devices
        _status.value = ScanStatus(
            phase = "done",
            progress = 1f,
            total = total,
            completed = total,
            found = devices.size,
        )
    }

    private fun findWifiNetwork(): Network? = runCatching {
        connectivity.allNetworks.firstOrNull { network ->
            connectivity.getNetworkCapabilities(network)
                ?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true
        }
    }.getOrNull()

    private fun probe(
        network: Network,
        ip: String,
        connectTimeoutMs: Int,
        readTimeoutMs: Int,
        allowNetworkStatusFallback: Boolean,
    ): DiscoveredDevice? {
        val cloud = readJson(
            network = network,
            url = "http://$ip/api/v1/cloud/status",
            connectTimeoutMs = connectTimeoutMs,
            readTimeoutMs = readTimeoutMs,
        )
        if (cloud?.optBoolean("ok", false) == true) {
            val deviceId = cloud.optString("deviceId").trim()
            if (deviceId.startsWith("HG-") && deviceId.length >= 5) {
                Log.i(TAG, "HomeGuard HTTP fallback found: id=$deviceId ip=$ip")
                return discovered(deviceId, ip)
            }
        }

        if (!allowNetworkStatusFallback) return null

        val networkStatus = readJson(
            network = network,
            url = "http://$ip/api/v1/network/status",
            connectTimeoutMs = connectTimeoutMs,
            readTimeoutMs = readTimeoutMs,
        ) ?: return null
        if (!networkStatus.optBoolean("ok", false)) return null

        val apSsid = networkStatus.optString("apSsid").trim()
        if (!apSsid.startsWith("HomeGuard-S3", ignoreCase = true)) return null

        val temporaryId = "setup-$ip-80"
        Log.i(TAG, "HomeGuard network-status fallback found: id=$temporaryId ip=$ip ap=$apSsid")
        return discovered(temporaryId, ip, serviceName = apSsid.ifBlank { "HomeGuard-S3" })
    }

    private fun readJson(
        network: Network,
        url: String,
        connectTimeoutMs: Int,
        readTimeoutMs: Int,
    ): JSONObject? {
        var connection: HttpURLConnection? = null
        return try {
            connection = network.openConnection(URL(url)) as HttpURLConnection
            connection.requestMethod = "GET"
            connection.connectTimeout = connectTimeoutMs
            connection.readTimeout = readTimeoutMs
            connection.useCaches = false
            connection.instanceFollowRedirects = false
            if (connection.responseCode != HttpURLConnection.HTTP_OK) return null
            val body = connection.inputStream.bufferedReader(Charsets.UTF_8).use(::readBoundedDiscoveryText) ?: return null
            JSONObject(body)
        } catch (_: Exception) {
            null
        } finally {
            connection?.disconnect()
        }
    }

    private fun discovered(
        deviceId: String,
        ip: String,
        serviceName: String = deviceId,
    ) = DiscoveredDevice(
        deviceId = deviceId,
        serviceName = serviceName,
        host = ip,
        port = 80,
        secure = false,
        apiVersion = 1,
        transport = Transport.NONE,
        pairingRequired = false,
        source = DiscoverySource.HTTP,
    )
}
