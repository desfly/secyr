package ua.homeguard.s3.provisioning

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.wifi.WifiNetworkSpecifier
import android.os.Build
import kotlinx.coroutines.suspendCancellableCoroutine
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

class SetupNetworkConnector(context: Context) {
    private val connectivity = context.applicationContext.getSystemService(ConnectivityManager::class.java)

    suspend fun connect(ssid: String, password: String): BoundSetupNetwork {
        require(Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            "На Android 8–9 підключіться до Setup AP вручну"
        }
        val specifier = WifiNetworkSpecifier.Builder().setSsid(ssid).setWpa2Passphrase(password).build()
        val request = NetworkRequest.Builder()
            .addTransportType(NetworkCapabilities.TRANSPORT_WIFI)
            .removeCapability(NetworkCapabilities.NET_CAPABILITY_INTERNET)
            .setNetworkSpecifier(specifier)
            .build()
        return suspendCancellableCoroutine { continuation ->
            val callback = object : ConnectivityManager.NetworkCallback() {
                override fun onAvailable(network: Network) {
                    connectivity.bindProcessToNetwork(network)
                    if (continuation.isActive) continuation.resume(BoundSetupNetwork(connectivity, network, this))
                }
                override fun onUnavailable() {
                    if (continuation.isActive) continuation.resumeWithException(IllegalStateException("Setup AP недоступна"))
                }
            }
            connectivity.requestNetwork(request, callback, 45_000)
            continuation.invokeOnCancellation { runCatching { connectivity.unregisterNetworkCallback(callback) } }
        }
    }
}

class BoundSetupNetwork(
    private val connectivity: ConnectivityManager,
    val network: Network,
    private val callback: ConnectivityManager.NetworkCallback
) : AutoCloseable {
    override fun close() {
        connectivity.bindProcessToNetwork(null)
        runCatching { connectivity.unregisterNetworkCallback(callback) }
    }
}
