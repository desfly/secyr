package ua.homeguard.s3.provisioning

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import ua.homeguard.s3.model.ProvisioningForm
import ua.homeguard.s3.model.ProvisioningQrData
import ua.homeguard.s3.network.PinnedTlsClientFactory

class PinnedProvisioningApi(private val qr: ProvisioningQrData) {
    private val client = PinnedTlsClientFactory.create(qr.certificateSha256)
    private val json = "application/json; charset=utf-8".toMediaType()

    suspend fun authorize() = post(
        "/v1/provisioning/authorize",
        JSONObject().put("pairing_code", qr.pairingCode).put("certificate_sha256", qr.certificateSha256)
    )

    suspend fun apply(form: ProvisioningForm, localApiToken: String) = post(
        "/v1/provisioning/apply",
        JSONObject()
            .put("wifi_ssid", form.wifiSsid)
            .put("wifi_password", form.wifiPassword)
            .put("owner_label", form.ownerLabel)
            .put("cloud_endpoint", form.cloudEndpoint)
            .put("cloud_token", form.cloudClaimToken)
            .put("local_api_token", localApiToken)
    )

    private suspend fun post(path: String, body: JSONObject) = withContext(Dispatchers.IO) {
        val request = Request.Builder().url(qr.setupUrl.trimEnd('/') + path)
            .post(body.toString().toRequestBody(json)).build()
        client.newCall(request).execute().use { response ->
            if (!response.isSuccessful) error("HomeGuard повернув HTTP ${response.code}")
            val payload = response.body?.string().orEmpty()
            if (payload.isNotBlank()) {
                val objectValue = JSONObject(payload)
                require(objectValue.optBoolean("ok", true)) { objectValue.optString("error", "Операцію відхилено") }
            }
        }
    }

}
