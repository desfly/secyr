package ua.homeguard.s3.diagnostics

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody

class ServiceMaintenanceClient(
    private val client: OkHttpClient,
    private val baseUrl: String,
) {
    suspend fun readiness(): ServiceReadiness = withContext(Dispatchers.IO) {
        val request = Request.Builder()
            .url(baseUrl.trimEnd('/') + "/api/v1/service/readiness")
            .get()
            .build()
        client.newCall(request).execute().use { response ->
            check(response.isSuccessful) { "Readiness HTTP ${response.code}" }
            ServiceReadiness.fromJson(response.body?.string().orEmpty())
        }
    }

    suspend fun invalidateCommissioning(): Boolean = withContext(Dispatchers.IO) {
        val request = Request.Builder()
            .url(baseUrl.trimEnd('/') + "/api/v1/service/invalidate")
            .post("".toRequestBody("application/json".toMediaType()))
            .build()
        client.newCall(request).execute().use { response -> response.isSuccessful }
    }
}
