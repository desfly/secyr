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

internal const val WIFI_SCAN_MAX_BODY_BYTES = 256_000

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
            socket.soTimeout = 8_000
            socket.connect(InetSocketAddress(host, port), 5_000)

            val request = buildString {
                append("GET ").append(path).append(" HTTP/1.0\r\n")
                append("Host: ").append(host)
                if (port != 80) append(':').append(port)
                append("\r\nAccept: application/json\r\nConnection: close\r\n\r\n")
            }
            socket.getOutputStream().apply {
                write(request.toByteArray(StandardCharsets.US_ASCII))
                flush()
            }

            val input = socket.getInputStream()
            val headerBytes = ByteArrayOutputStream()
            var state = 0
            while (state < 4) {
                val value = input.read()
                if (value < 0) error("HomeGuard закрив з’єднання до HTTP-заголовків")
                headerBytes.write(value)
                state = when {
                    state == 0 && value == '\r'.code -> 1
                    state == 1 && value == '\n'.code -> 2
                    state == 2 && value == '\r'.code -> 3
                    state == 3 && value == '\n'.code -> 4
                    value == '\r'.code -> 1
                    else -> 0
                }
                if (headerBytes.size() > 16_384) error("Завеликі HTTP-заголовки HomeGuard")
            }

            val header = headerBytes.toString(StandardCharsets.ISO_8859_1.name())
            val statusLine = header.lineSequence().firstOrNull().orEmpty()
            val statusCode = statusLine.split(' ').getOrNull(1)?.toIntOrNull()
                ?: error("HomeGuard повернув некоректний HTTP-статус")
            if (statusCode !in 200..299) error("HomeGuard повернув HTTP $statusCode")

            val contentLength = header.lineSequence()
                .firstOrNull { it.startsWith("Content-Length:", ignoreCase = true) }
                ?.substringAfter(':')?.trim()?.toIntOrNull()

            val bodyBytes = ByteArrayOutputStream()
            if (contentLength != null && contentLength >= 0) {
                if (contentLength > WIFI_SCAN_MAX_BODY_BYTES) error("Завелика відповідь Wi-Fi scan")
                val buffer = ByteArray(2048)
                var remaining = contentLength
                while (remaining > 0) {
                    val count = input.read(buffer, 0, minOf(buffer.size, remaining))
                    if (count < 0) error("HomeGuard передав неповний Wi-Fi scan")
                    if (count > 0) {
                        bodyBytes.write(buffer, 0, count)
                        remaining -= count
                    }
                }
            } else {
                val buffer = ByteArray(2048)
                var jsonStarted = false
                var depth = 0
                var inString = false
                var escaped = false
                while (true) {
                    val count = input.read(buffer)
                    if (count < 0) break
                    for (i in 0 until count) {
                        val b = buffer[i]
                        bodyBytes.write(b.toInt())
                        if (bodyBytes.size() > WIFI_SCAN_MAX_BODY_BYTES) error("Завелика відповідь Wi-Fi scan")
                        val ch = b.toInt().toChar()
                        if (!jsonStarted) {
                            if (ch == '{') { jsonStarted = true; depth = 1 }
                            continue
                        }
                        if (inString) {
                            if (escaped) escaped = false
                            else if (ch == '\\') escaped = true
                            else if (ch == '"') inString = false
                        } else {
                            when (ch) {
                                '"' -> inString = true
                                '{', '[' -> depth++
                                '}', ']' -> depth--
                            }
                        }
                        if (jsonStarted && depth == 0) break
                    }
                    if (jsonStarted && depth == 0) break
                }
            }

            val rawBody = bodyBytes.toString(StandardCharsets.UTF_8.name()).trim()
            val start = rawBody.indexOf('{')
            val end = rawBody.lastIndexOf('}')
            if (start < 0 || end < start) error("HomeGuard не повернув JSON Wi-Fi scan")
            return rawBody.substring(start, end + 1)
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
