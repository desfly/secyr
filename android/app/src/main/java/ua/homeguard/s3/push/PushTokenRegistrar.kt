package ua.homeguard.s3.push

import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import ua.homeguard.s3.auth.CloudAccountAuth
import ua.homeguard.s3.storage.AppSettings
import java.io.IOException

object PushTokenRegistrar {
    private val client = OkHttpClient()

    suspend fun register(settings: AppSettings, fcmToken: String) = withContext(Dispatchers.IO) {
        val root = settings.cloudBaseUrl.trimEnd('/')
        val deviceId = settings.deviceId.trim()
        if (root.isBlank() || deviceId.isBlank() || fcmToken.isBlank()) {
            throw IOException("push_registration_not_configured")
        }
        val bearer = CloudAccountAuth.idToken()
        val body = JSONObject().put("token", fcmToken).toString()
            .toRequestBody("application/json; charset=utf-8".toMediaType())
        val request = Request.Builder()
            .url("$root/v1/devices/$deviceId/push-tokens")
            .header("Accept", "application/json")
            .header("Authorization", "Bearer $bearer")
            .post(body)
            .build()
        client.newCall(request).execute().use { reply ->
            if (!reply.isSuccessful) throw IOException("push_registration_http_${reply.code}")
        }
    }
}
