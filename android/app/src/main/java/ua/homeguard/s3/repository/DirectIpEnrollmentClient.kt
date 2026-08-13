package ua.homeguard.s3.repository

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import ua.homeguard.s3.model.AccessLoginRejectedException
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.DiscoverySource
import ua.homeguard.s3.model.Transport
import ua.homeguard.s3.model.accessLoginFailureReason
import ua.homeguard.s3.storage.SavedUserCredentials
import java.io.IOException
import java.net.URI
import java.util.concurrent.TimeUnit

class DirectIpEnrollmentClient(
    private val client: OkHttpClient = OkHttpClient.Builder()
        .connectTimeout(2, TimeUnit.SECONDS)
        .readTimeout(3, TimeUnit.SECONDS)
        .writeTimeout(3, TimeUnit.SECONDS)
        .build(),
) {
    suspend fun resolveAndAuthorize(
        rawAddress: String,
        credentials: SavedUserCredentials,
    ): DiscoveredDevice = withContext(Dispatchers.IO) {
        val address = normalizeAddress(rawAddress)
        var lastError: Throwable? = null

        for ((secure, baseUrl) in candidateUrls(address)) {
            try {
                val json = login(baseUrl, credentials)
                val uri = URI(baseUrl)
                val defaultPort = if (secure) 443 else 80
                val port = if (uri.port > 0) uri.port else defaultPort
                val deviceId = json.optString("deviceId")
                    .ifBlank { json.optString("device_id") }
                    .ifBlank { "manual:${uri.host.lowercase()}:$port" }

                return@withContext DiscoveredDevice(
                    deviceId = deviceId,
                    serviceName = "HomeGuard",
                    host = uri.host,
                    port = port,
                    secure = secure,
                    apiVersion = json.optInt("apiVersion", 1),
                    transport = Transport.NONE,
                    pairingRequired = false,
                    source = DiscoverySource.UDP,
                )
            } catch (error: AccessLoginRejectedException) {
                throw error
            } catch (error: Throwable) {
                lastError = error
            }
        }
        throw IOException("HomeGuard not found at $address", lastError)
    }

    private fun login(baseUrl: String, credentials: SavedUserCredentials): JSONObject {
        val body = JSONObject()
            .put("actor", credentials.username)
            .put("credential", credentials.password)
            .toString()
            .toRequestBody("application/json; charset=utf-8".toMediaType())
        val request = Request.Builder()
            .url(baseUrl.trimEnd('/') + "/api/v1/access/login")
            .header("Accept", "application/json")
            .post(body)
            .build()

        client.newCall(request).execute().use { response ->
            val text = response.body?.string().orEmpty()
            if (!response.isSuccessful) throw IOException("HTTP ${response.code}")
            val json = if (text.isBlank()) JSONObject() else JSONObject(text)
            if (!json.optBoolean("ok", false)) {
                val rawReason = json.optString("reason", "unknown")
                throw AccessLoginRejectedException(
                    accessLoginFailureReason(rawReason),
                    "Login rejected: $rawReason",
                )
            }
            return json
        }
    }

    private fun candidateUrls(address: String): List<Pair<Boolean, String>> {
        if (address.startsWith("http://", true)) return listOf(false to address.trimEnd('/'))
        if (address.startsWith("https://", true)) return listOf(true to address.trimEnd('/'))
        return listOf(
            false to "http://$address",
            true to "https://$address",
        )
    }

    private fun normalizeAddress(value: String): String {
        val normalized = value.trim().trimEnd('/')
        require(normalized.isNotEmpty()) { "IP address is required" }
        require(!normalized.contains(' ')) { "Invalid IP address" }
        return normalized
    }
}
