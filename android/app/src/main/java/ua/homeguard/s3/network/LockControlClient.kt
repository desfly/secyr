package ua.homeguard.s3.network

import kotlinx.coroutines.suspendCancellableCoroutine
import okhttp3.Call
import okhttp3.Callback
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import okhttp3.Response
import org.json.JSONObject
import java.io.IOException
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

class LockControlClient(
    baseUrl: String,
    private val sessionToken: String,
    certificatePin: String,
) {
    private val root = baseUrl.trimEnd('/')
    private val client = PinnedTlsClientFactory.create(certificatePin)

    suspend fun pulse(actor: String): Int {
        require(root.isNotBlank()) { "controller offline" }
        require(sessionToken.isNotBlank()) { "authorization required" }
        require(actor.isNotBlank()) { "actor required" }

        val body = JSONObject().put("actor", actor.trim()).toString()
            .toRequestBody("application/json; charset=utf-8".toMediaType())
        val request = Request.Builder()
            .url(root + "/api/v1/outputs/lock/pulse")
            .header("Accept", "application/json")
            .header("Authorization", "Bearer $sessionToken")
            .post(body)
            .build()

        return client.newCall(request).await().use { response ->
            val text = response.body?.string().orEmpty()
            if (!response.isSuccessful) throw IOException("HTTP ${response.code}: $text")
            val json = if (text.isBlank()) JSONObject() else JSONObject(text)
            if (!json.optBoolean("ok", false)) throw IOException("Lock rejected: ${json.optString("reason", "unknown")}")
            json.optInt("pulseMs", 5000)
        }
    }

    private suspend fun Call.await(): Response = suspendCancellableCoroutine { continuation ->
        continuation.invokeOnCancellation { cancel() }
        enqueue(object : Callback {
            override fun onFailure(call: Call, error: IOException) {
                if (continuation.isActive) continuation.resumeWithException(error)
            }

            override fun onResponse(call: Call, response: Response) {
                if (continuation.isActive) continuation.resume(response) else response.close()
            }
        })
    }
}
