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
import java.net.HttpURLConnection
import java.net.Inet4Address
import java.net.URL

class HttpSubnetDiscovery(context: Context) {
    companion object {
        private const val TAG = "HomeGuardHttpScan"
        private const val CONNECT_TIMEOUT_MS = 220
        private const val READ_TIMEOUT_MS = 320
        private const val MAX_PARALLEL = 32
    }

    private val connectivity = context.applicationContext.getSystemService(ConnectivityManager::class.java)
    private val _devices = MutableStateFlow<List<DiscoveredDevice>>(emptyList())
    val devices: StateFlow<List<DiscoveredDevice>> = _devices.asStateFlow()

    suspend fun scanOnce() = withContext(Dispatchers.IO) {
        val network = findWifiNetwork() ?: return@withContext
        val local = connectivity.getLinkProperties(network)
            ?.linkAddresses
            ?.map { it.address }
            ?.filterIsInstance<Inet4Address>()
            ?.firstOrNull { !it.isLoopbackAddress && !it.isLinkLocalAddress }
            ?: return@withContext

        // HomeGuard installations are expected on a normal home LAN. Scan the local /24
        // as a deterministic fallback when multicast/broadcast is filtered by Android/AP.
        val bytes = local.address
        val prefix = "${bytes[0].toInt() and 0xff}.${bytes[1].toInt() and 0xff}.${bytes[2].toInt() and 0xff}"
        val ownHost = bytes[3].toInt() and 0xff
        val semaphore = Semaphore(MAX_PARALLEL)

        val found = coroutineScope {
            (1..254)
                .filter { it != ownHost }
                .map { host ->
                    async(Dispatchers.IO) {
                        semaphore.withPermit { probe(network, "$prefix.$host") }
                    }
                }
                .awaitAll()
                .filterNotNull()
        }

        _devices.value = found
            .associateBy { it.deviceId }
            .values
            .sortedBy { it.deviceId }
    }

    private fun findWifiNetwork(): Network? = runCatching {
        connectivity.allNetworks.firstOrNull { network ->
            connectivity.getNetworkCapabilities(network)
                ?.hasTransport(NetworkCapabilities.TRANSPORT_WIFI) == true
        }
    }.getOrNull()

    private fun probe(network: Network, ip: String): DiscoveredDevice? {
        var connection: HttpURLConnection? = null
        return try {
            connection = network.openConnection(URL("http://$ip/api/v1/cloud/status")) as HttpURLConnection
            connection.requestMethod = "GET"
            connection.connectTimeout = CONNECT_TIMEOUT_MS
            connection.readTimeout = READ_TIMEOUT_MS
            connection.useCaches = false
            connection.instanceFollowRedirects = false
            if (connection.responseCode != HttpURLConnection.HTTP_OK) return null

            val body = connection.inputStream.bufferedReader(Charsets.UTF_8).use { it.readText() }
            val json = JSONObject(body)
            if (!json.optBoolean("ok", false)) return null
            val deviceId = json.optString("deviceId")
            if (!deviceId.startsWith("HG-") || deviceId.length < 5) return null

            Log.i(TAG, "HomeGuard HTTP fallback found: id=$deviceId ip=$ip")
            DiscoveredDevice(
                deviceId = deviceId,
                serviceName = deviceId,
                host = ip,
                port = 80,
                secure = false,
                apiVersion = 1,
                transport = Transport.NONE,
                pairingRequired = false,
                source = DiscoverySource.HTTP,
            )
        } catch (_: Exception) {
            null
        } finally {
            connection?.disconnect()
        }
    }
}
