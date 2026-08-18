package ua.homeguard.s3.network

import kotlinx.coroutines.suspendCancellableCoroutine
import okhttp3.Call
import okhttp3.Callback
import okhttp3.EventListener
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import okhttp3.Response
import org.json.JSONObject
import java.io.IOException
import java.util.concurrent.atomic.AtomicBoolean
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

enum class FactoryResetResult {
    ACCEPTED,
    REJECTED,
    CONNECTION_LOST,
}

internal fun classifyFactoryResetIOException(requestBodySent: Boolean): FactoryResetResult =
    if (requestBodySent) FactoryResetResult.CONNECTION_LOST else FactoryResetResult.REJECTED

class FactoryResetClient(
    baseUrl: String,
    certificatePin: String = "",
) {
    private val root = baseUrl.trimEnd('/')
    private val client = PinnedTlsClientFactory.create(certificatePin)

    suspend fun reset(actor: String, credential: String): FactoryResetResult {
        val normalizedActor = actor.trim()
        require(normalizedActor.isNotBlank()) { "Admin ID is required" }
        require(credential.length in 4..12 && credential.all(Char::isDigit)) { "PIN must contain 4-12 digits" }
        require(root.isNotBlank()) { "Local controller endpoint is unavailable" }

        val body = JSONObject()
            .put("actor", normalizedActor)
            .put("credential", credential)
            .put("confirm", "ERASE_ALL")

        val request = Request.Builder()
            .url(root + RuntimeApiContract.FACTORY_RESET_PATH)
            .header("Accept", "application/json")
            .header("Cache-Control", "no-store")
            .post(body.toString().toRequestBody("application/json; charset=utf-8".toMediaType()))
            .build()

        val requestBodySent = AtomicBoolean(false)
        val trackedClient = client.newBuilder()
            .eventListener(object : EventListener() {
                override fun requestBodyEnd(call: Call, byteCount: Long) {
                    requestBodySent.set(true)
                }
            })
            .build()

        val response = try {
            trackedClient.newCall(request).await()
        } catch (_: IOException) {
            // A reset controller may erase Wi-Fi and reboot after it has already
            // received the destructive request. Only classify transport loss as an
            // expected reset disconnect when the request body was actually sent.
            // DNS/connect/TLS failures before submission must not clear local state
            // as if the reset had succeeded.
            return classifyFactoryResetIOException(requestBodySent.get())
        }

        response.use {
            val text = it.body?.string().orEmpty()
            if (!it.isSuccessful) return FactoryResetResult.REJECTED
            val json = runCatching { if (text.isBlank()) JSONObject() else JSONObject(text) }.getOrNull()
                ?: return FactoryResetResult.REJECTED
            return if (json.optBoolean("ok", false) && json.optBoolean("rebooting", false)) {
                FactoryResetResult.ACCEPTED
            } else {
                FactoryResetResult.REJECTED
            }
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
