package ua.homeguard.s3.provisioning

import android.content.Context
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.net.wifi.WifiNetworkSpecifier
import android.os.Build
import kotlinx.coroutines.suspendCancellableCoroutine
import java.util.concurrent.atomic.AtomicBoolean
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
            val terminal = AtomicBoolean(false)
            val callback = object : ConnectivityManager.NetworkCallback() {
                override fun onAvailable(network: Network) {
                    if (!terminal.compareAndSet(false, true)) return
                    val bound = BoundSetupNetwork(connectivity, network, this)
                    runCatching { connectivity.bindProcessToNetwork(network) }
                        .onSuccess {
                            continuation.resume(bound) { _, unclaimed, _ -> unclaimed.close() }
                        }
                        .onFailure { error ->
                            bound.close()
                            continuation.resumeWithException(error)
                        }
                }

                override fun onUnavailable() {
                    if (!terminal.compareAndSet(false, true)) return
                    continuation.resumeWithException(IllegalStateException("Setup AP недоступна"))
                }
            }
            connectivity.requestNetwork(request, callback, 45_000)
            continuation.invokeOnCancellation {
                if (terminal.compareAndSet(false, true)) {
                    runCatching { connectivity.unregisterNetworkCallback(callback) }
                }
            }
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
