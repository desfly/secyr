package ua.homeguard.s3.provisioning

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject

class SoftApProvisioningApi(
    private val baseUrl: String = "http://192.168.4.1",
    private val client: OkHttpClient = OkHttpClient(),
) {
    private val json = "application/json; charset=utf-8".toMediaType()

    suspend fun applyWifi(ssid: String, password: String): JSONObject = withContext(Dispatchers.IO) {
        require(ssid.isNotBlank()) { "SSID не може бути порожнім" }
        val payload = JSONObject()
            .put("ssid", ssid)
            .put("password", password)
        val request = Request.Builder()
            .url(baseUrl.trimEnd('/') + "/api/v1/provisioning/wifi")
            .post(payload.toString().toRequestBody(json))
            .build()
        client.newCall(request).execute().use { response ->
            val body = response.body?.string().orEmpty()
            if (!response.isSuccessful) error("HomeGuard повернув HTTP ${response.code}: $body")
            if (body.isBlank()) JSONObject() else JSONObject(body)
        }
    }

    suspend fun status(): JSONObject = withContext(Dispatchers.IO) {
        val request = Request.Builder()
            .url(baseUrl.trimEnd('/') + "/api/v1/wifi/status")
            .get()
            .build()
        client.newCall(request).execute().use { response ->
            val body = response.body?.string().orEmpty()
            if (!response.isSuccessful) error("HomeGuard повернув HTTP ${response.code}: $body")
            if (body.isBlank()) JSONObject() else JSONObject(body)
        }
    }
}
