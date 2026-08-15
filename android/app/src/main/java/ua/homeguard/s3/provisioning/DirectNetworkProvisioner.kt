package ua.homeguard.s3.provisioning

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URI
import java.net.URL

object DirectNetworkProvisioner {
    suspend fun connect(
        rawAddress: String,
        actor: String,
        credential: String,
        ssid: String,
        password: String,
    ): String = withContext(Dispatchers.IO) {
        require(actor.isNotBlank()) { "Вкажіть користувача HomeGuard" }
        require(credential.length in 4..12 && credential.all(Char::isDigit)) { "PIN HomeGuard має містити 4–12 цифр" }
        require(ssid.isNotBlank()) { "Вкажіть домашню Wi-Fi мережу" }
        require(password.length in 8..64) { "Перевірте пароль домашньої Wi-Fi мережі" }

        val base = normalize(rawAddress) ?: error("Некоректна адреса HomeGuard")
        val connection = (URL("$base/api/v1/network/connect").openConnection() as HttpURLConnection).apply {
            requestMethod = "POST"
            connectTimeout = 5_000
            readTimeout = 8_000
            doOutput = true
            useCaches = false
            instanceFollowRedirects = false
            setRequestProperty("Content-Type", "application/json; charset=utf-8")
            setRequestProperty("Accept", "application/json")
            setRequestProperty("Connection", "close")
        }

        try {
            val request = JSONObject()
                .put("actor", actor.trim())
                .put("credential", credential)
                .put("ssid", ssid)
                .put("password", password)
                .toString()
            connection.outputStream.use { output ->
                output.write(request.toByteArray(Charsets.UTF_8))
                output.flush()
            }

            val status = connection.responseCode
            val stream = if (status in 200..299) connection.inputStream else connection.errorStream
            val body = stream?.bufferedReader(Charsets.UTF_8)?.use { it.readText() }.orEmpty()
            val json = runCatching { JSONObject(body) }.getOrNull()
            if (status !in 200..299 || json?.optBoolean("ok", false) != true) {
                val reason = json?.optString("reason")?.takeIf { it.isNotBlank() }
                    ?: "HTTP $status"
                error("HomeGuard відхилив Wi-Fi налаштування: $reason")
            }
            json.optString("state", "connecting")
        } finally {
            connection.disconnect()
        }
    }

    private fun normalize(raw: String): String? {
        val value = raw.trim().trimEnd('/')
        if (value.isEmpty() || value.any(Char::isWhitespace)) return null
        val withScheme = if (value.startsWith("http://", true) || value.startsWith("https://", true)) value else "http://$value"
        val uri = runCatching { URI(withScheme) }.getOrNull() ?: return null
        if (!uri.scheme.equals("http", true)) return null
        if (uri.host.isNullOrBlank()) return null
        if (uri.port == 0 || uri.port > 65535) return null
        if (!uri.path.isNullOrBlank() && uri.path != "/") return null
        if (!uri.query.isNullOrBlank() || !uri.fragment.isNullOrBlank() || !uri.userInfo.isNullOrBlank()) return null
        return withScheme.trimEnd('/')
    }
}
