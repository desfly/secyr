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

enum class FactoryResetResult {
    ACCEPTED,
    REJECTED,
    CONNECTION_LOST,
}

class FactoryResetClient(
    baseUrl: String,
    private val sessionToken: String,
    certificatePin: String = "",
) {
    private val root = baseUrl.trimEnd('/')
    private val client = PinnedTlsClientFactory.create(certificatePin)

    suspend fun reset(actor: String): FactoryResetResult {
        val normalizedActor = actor.trim()
        require(normalizedActor.isNotBlank()) { "Admin ID is required" }
        require(sessionToken.matches(Regex("^[0-9a-f]{64}$"))) { "Admin Bearer session is required" }
        require(root.isNotBlank()) { "Local controller endpoint is unavailable" }

        val body = JSONObject()
            .put("actor", normalizedActor)
            .put("confirm", "ERASE_ALL")

        val request = Request.Builder()
            .url(root + RuntimeApiContract.FACTORY_RESET_PATH)
            .header("Accept", "application/json")
            .header("Cache-Control", "no-store")
            .header("Authorization", "Bearer $sessionToken")
            .post(body.toString().toRequestBody("application/json; charset=utf-8".toMediaType()))
            .build()

        val response = try {
            client.newCall(request).await()
        } catch (_: IOException) {
            // The controller may erase Wi-Fi state and reboot before the HTTP
            // response reaches Android. After the destructive request has been
            // submitted, transport loss must fail closed locally.
            return FactoryResetResult.CONNECTION_LOST
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

    /* LEGACY v1 actor+PIN reset path intentionally removed from active use.
       The old model remains documented in history until the v2 rollout is
       proven; runtime reset now authenticates only with the login Bearer. */

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
