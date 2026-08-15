package ua.homeguard.s3.network

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject
import java.net.URI
import java.util.concurrent.TimeUnit

data class HomeGuardWifiNetwork(
    val ssid: String,
    val rssi: Int,
)

object HomeGuardWifiScanner {
    private val client = OkHttpClient.Builder()
        .connectTimeout(5, TimeUnit.SECONDS)
        .readTimeout(20, TimeUnit.SECONDS)
        .callTimeout(25, TimeUnit.SECONDS)
        .build()

    suspend fun scan(rawAddress: String): List<HomeGuardWifiNetwork> = withContext(Dispatchers.IO) {
        val baseUrl = normalizeLocalAddress(rawAddress)
            ?: error("Некоректна адреса HomeGuard")
        val request = Request.Builder()
            .url("$baseUrl/api/v1/network/scan")
            .get()
            .build()

        client.newCall(request).execute().use { response ->
            if (!response.isSuccessful) error("HomeGuard повернув HTTP ${response.code}")
            val payload = JSONObject(response.body?.string().orEmpty())
            if (!payload.optBoolean("ok", false)) {
                error(payload.optString("reason", "Сканування Wi-Fi відхилено"))
            }

            val networks = payload.optJSONArray("networks") ?: return@use emptyList()
            val strongestBySsid = LinkedHashMap<String, Int>()
            for (index in 0 until networks.length()) {
                val item = networks.optJSONObject(index) ?: continue
                val ssid = item.optString("ssid").trim()
                if (ssid.isBlank()) continue
                val rssi = item.optInt("rssi", -127)
                val previous = strongestBySsid[ssid]
                if (previous == null || rssi > previous) strongestBySsid[ssid] = rssi
            }
            strongestBySsid.entries
                .sortedByDescending { it.value }
                .map { HomeGuardWifiNetwork(it.key, it.value) }
        }
    }

    private fun normalizeLocalAddress(raw: String): String? {
        val value = raw.trim().trimEnd('/')
        if (value.isEmpty() || value.any(Char::isWhitespace)) return null
        val withScheme = when {
            value.startsWith("http://", ignoreCase = true) || value.startsWith("https://", ignoreCase = true) -> value
            else -> "http://$value"
        }
        val uri = runCatching { URI(withScheme) }.getOrNull() ?: return null
        val scheme = uri.scheme?.lowercase()
        if (scheme != "http" && scheme != "https") return null
        if (uri.host.isNullOrBlank()) return null
        if (uri.port == 0 || uri.port > 65535) return null
        if (!uri.path.isNullOrBlank() && uri.path != "/") return null
        if (!uri.query.isNullOrBlank() || !uri.fragment.isNullOrBlank() || !uri.userInfo.isNullOrBlank()) return null
        return withScheme.trimEnd('/')
    }
}
