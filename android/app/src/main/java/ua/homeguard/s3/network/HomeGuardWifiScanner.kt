package ua.homeguard.s3.network

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.ByteArrayOutputStream
import java.net.InetSocketAddress
import java.net.URI
import java.net.Socket
import java.nio.charset.StandardCharsets

data class HomeGuardWifiNetwork(
    val ssid: String,
    val rssi: Int,
)

object HomeGuardWifiScanner {
    suspend fun scan(rawAddress: String): List<HomeGuardWifiNetwork> = withContext(Dispatchers.IO) {
        val uri = normalizeLocalUri(rawAddress)
            ?: error("Некоректна адреса HomeGuard")
        if (!uri.scheme.equals("http", ignoreCase = true)) {
            error("Wi-Fi scan HomeGuard підтримує локальний HTTP")
        }

        val host = uri.host ?: error("Некоректна адреса HomeGuard")
        val port = if (uri.port == -1) 80 else uri.port
        val body = rawHttpGet(host, port, "/api/v1/network/scan")
        val payload = JSONObject(body)
        if (!payload.optBoolean("ok", false)) {
            error(payload.optString("reason", "Сканування Wi-Fi відхилено"))
        }

        val networks = payload.optJSONArray("networks") ?: return@withContext emptyList()
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

    private fun rawHttpGet(host: String, port: Int, path: String): String {
        Socket().use { socket ->
            socket.soTimeout = 25_000
            socket.connect(InetSocketAddress(host, port), 5_000)

            val request = buildString {
                append("GET ").append(path).append(" HTTP/1.0\r\n")
                append("Host: ").append(host)
                if (port != 80) append(':').append(port)
                append("\r\n")
                append("Accept: application/json\r\n")
                append("Connection: close\r\n")
                append("\r\n")
            }
            socket.getOutputStream().apply {
                write(request.toByteArray(StandardCharsets.US_ASCII))
                flush()
            }

            val bytes = ByteArrayOutputStream()
            val buffer = ByteArray(4096)
            val input = socket.getInputStream()
            while (true) {
                val count = input.read(buffer)
                if (count < 0) break
                if (count > 0) bytes.write(buffer, 0, count)
            }

            val response = bytes.toString(StandardCharsets.UTF_8.name())
            val separator = response.indexOf("\r\n\r\n")
            if (separator < 0) error("HomeGuard повернув некоректну HTTP-відповідь")

            val header = response.substring(0, separator)
            val statusLine = header.lineSequence().firstOrNull().orEmpty()
            val statusCode = statusLine.split(' ').getOrNull(1)?.toIntOrNull()
                ?: error("HomeGuard повернув некоректний HTTP-статус")
            if (statusCode !in 200..299) error("HomeGuard повернув HTTP $statusCode")

            val body = response.substring(separator + 4).trim()
            if (body.isEmpty()) error("HomeGuard повернув порожній результат Wi-Fi scan")
            return body
        }
    }

    private fun normalizeLocalUri(raw: String): URI? {
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
        return uri
    }
}
