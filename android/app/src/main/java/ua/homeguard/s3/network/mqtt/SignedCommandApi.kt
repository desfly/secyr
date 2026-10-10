package ua.homeguard.s3.network.mqtt

import kotlinx.coroutines.suspendCancellableCoroutine
import okhttp3.Call
import okhttp3.Callback
import okhttp3.HttpUrl.Companion.toHttpUrl
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import okhttp3.Response
import org.json.JSONObject
import java.io.IOException
import java.util.concurrent.TimeUnit
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

/** Fetches a server-signed packet; never publishes it or retries an uncertain command. */
class SignedCommandApi(
    baseUrl: String,
    private val tokenProvider: () -> String,
    client: OkHttpClient = OkHttpClient(),
) {
    private val root = baseUrl.toHttpUrl().also {
        require(it.isHttps && it.username.isEmpty() && it.password.isEmpty() && it.query == null && it.fragment == null) {
            "Signer requires an HTTPS service URL"
        }
    }
    private val http = client.newBuilder().followRedirects(false).followSslRedirects(false)
        .retryOnConnectionFailure(false).callTimeout(15, TimeUnit.SECONDS).build()

    suspend fun request(deviceId: String, command: String, challenge: String = ""): SignedCommandEnvelope {
        require(deviceId.matches(Regex("[A-Za-z0-9_-]+"))) { "Invalid signer device id" }
        require(command in setOf("security.arm_home", "security.arm_away", "security.disarm",
            "security.disarm_challenge", "security.panic", "output.lock")) { "Unsupported signed command" }
        require(if (command == "security.disarm") challenge.matches(Regex("[0-9a-fA-F]{32}")) else challenge.isEmpty()) {
            "Invalid signed command challenge"
        }
        val token = tokenProvider()
        require(token.isNotBlank() && token.length <= 4096 && token.none { it.code < 32 || it.code == 127 }) {
            "Signer account authorization required"
        }
        val url = root.newBuilder().addPathSegments("v1/devices").addPathSegment(deviceId)
            .addPathSegment("signed-command").build()
        val body = JSONObject().put("command", command).put("challenge", challenge).toString()
        val request = Request.Builder().url(url).header("Authorization", "Bearer $token")
            .post(body.toRequestBody("application/json".toMediaType())).build()
        return suspendCancellableCoroutine { continuation ->
            val call = http.newCall(request)
            continuation.invokeOnCancellation { call.cancel() }
            call.enqueue(object : Callback {
                override fun onFailure(call: Call, error: IOException) {
                    if (continuation.isActive) continuation.resumeWithException(IOException("Signer request failed", error))
                }
                override fun onResponse(call: Call, response: Response) {
                    val result = runCatching {
                        response.use {
                            if (!it.isSuccessful) throw IOException("Signer HTTP ${it.code}")
                            val source = it.body?.source() ?: throw IOException("Signer response missing")
                            source.request(16_385)
                            if (source.buffer.size > 16_384) throw IOException("Signer response too large")
                            val json = JSONObject(source.readUtf8())
                            if (json.optString("command") != command || json.optString("challenge") != challenge) {
                                throw IOException("Signer command mismatch")
                            }
                            SignedCommandEnvelope.parse(json).also { envelope -> envelope.payloadFor(deviceId) }
                        }
                    }
                    if (continuation.isActive) result.fold(continuation::resume, continuation::resumeWithException)
                }
            })
        }
    }
}
