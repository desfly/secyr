package ua.homeguard.s3.control

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody

class OutputControlClient(
    private val baseUrl: String,
    private val client: OkHttpClient = OkHttpClient(),
) {
    data class Result(
        val httpCode: Int,
        val accepted: Boolean,
        val body: String,
    )

    suspend fun setOutput(outputId: Int, active: Boolean, alarmActive: Boolean = false): Result =
        withContext(Dispatchers.IO) {
            require(outputId in 1..65535)
            val json = "{\"outputId\":$outputId,\"active\":$active,\"alarmActive\":$alarmActive}"
            val request = Request.Builder()
                .url(baseUrl.trimEnd('/') + "/api/v1/system/output-command")
                .post(json.toRequestBody("application/json".toMediaType()))
                .build()

            client.newCall(request).execute().use { response ->
                val body = response.body?.string().orEmpty()
                Result(response.code, response.isSuccessful, body)
            }
        }
}
