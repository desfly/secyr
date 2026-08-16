package ua.homeguard.s3.diagnostics

import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.suspendCancellableCoroutine
import okhttp3.Call
import okhttp3.Callback
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import okhttp3.Response
import org.json.JSONObject
import ua.homeguard.s3.model.AccessRole
import ua.homeguard.s3.model.AccessSession
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.network.PinnedTlsClientFactory
import ua.homeguard.s3.network.RuntimeApiContract
import java.io.IOException
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

class FactoryResetTransportException(cause: IOException) :
    IOException("controller disconnected while Factory Reset was being submitted", cause)

class FactoryResetRejectedException(message: String) : IOException(message)

class DeviceConfigMaintenanceClient(
    private val endpoint: StateFlow<DeviceEndpoint>,
) {
    suspend fun exportConfig(session: AccessSession, credential: String): String {
        requireAdmin(session, credential)
        val target = localTarget()
        return post(
            target,
            RuntimeApiContract.CONFIG_EXPORT_PATH,
            JSONObject()
                .put("actor", session.actor)
                .put("credential", credential)
                .put("confirm", "INCLUDE_SECRETS"),
        ).use { response ->
            val text = response.body?.string().orEmpty()
            if (!response.isSuccessful) throw IOException("HTTP ${response.code}: $text")
            val backup = JSONObject(text)
            require(backup.optString("format") == "homeguard-config") { "unsupported backup format" }
            require(backup.optInt("version", -1) == 1) { "unsupported backup version" }
            require(backup.optBoolean("secretsIncluded", false)) { "backup is not restorable" }
            text
        }
    }

    suspend fun importConfig(session: AccessSession, credential: String, backupText: String) {
        requireAdmin(session, credential)
        val backup = JSONObject(backupText)
        require(backup.optString("format") == "homeguard-config") { "unsupported backup format" }
        require(backup.optInt("version", -1) == 1) { "unsupported backup version" }
        require(backup.optBoolean("secretsIncluded", false)) { "restorable secrets required" }

        val target = localTarget()
        post(
            target,
            RuntimeApiContract.CONFIG_IMPORT_PATH,
            JSONObject()
                .put("actor", session.actor)
                .put("credential", credential)
                .put("confirm", "APPLY_CONFIG")
                .put("backup", backup),
        ).use { response ->
            val text = response.body?.string().orEmpty()
            if (!response.isSuccessful) throw IOException("HTTP ${response.code}: $text")
            val result = if (text.isBlank()) JSONObject() else JSONObject(text)
            if (!result.optBoolean("ok", false)) {
                throw IOException(result.optString("reason", "config import rejected"))
            }
        }
    }

    suspend fun factoryReset(session: AccessSession, credential: String) {
        requireAdmin(session, credential)
        val target = localTarget()
        val response = try {
            post(
                target,
                RuntimeApiContract.FACTORY_RESET_PATH,
                JSONObject()
                    .put("actor", session.actor)
                    .put("credential", credential)
                    .put("confirm", "ERASE_ALL"),
            )
        } catch (error: IOException) {
            // A reset is intentionally connection-destructive. Distinguish a
            // transport loss from an explicit HTTP rejection so the caller can
            // fail closed instead of showing stale authorized/online state.
            throw FactoryResetTransportException(error)
        }

        response.use {
            val text = it.body?.string().orEmpty()
            if (!it.isSuccessful) {
                throw FactoryResetRejectedException("HTTP ${it.code}: $text")
            }
            val result = if (text.isBlank()) JSONObject() else JSONObject(text)
            if (!result.optBoolean("ok", false)) {
                throw FactoryResetRejectedException(result.optString("reason", "factory reset rejected"))
            }
            if (!result.optBoolean("rebooting", false)) {
                throw FactoryResetRejectedException("factory reset did not confirm reboot")
            }
        }
    }

    private fun requireAdmin(session: AccessSession, credential: String) {
        require(session.role == AccessRole.ADMIN) { "Admin role required" }
        require(session.actor.isNotBlank()) { "Admin ID required" }
        require(credential.length in 4..12 && credential.all(Char::isDigit)) { "PIN must contain 4-12 digits" }
    }

    private fun localTarget(): DeviceEndpoint {
        val target = endpoint.value
        require(target.path != ControlPath.OFFLINE && target.path != ControlPath.CLOUD) {
            "local controller connection required"
        }
        require(target.apiBaseUrl.isNotBlank()) { "controller endpoint unavailable" }
        return target
    }

    private suspend fun post(target: DeviceEndpoint, path: String, body: JSONObject): Response {
        val client = PinnedTlsClientFactory.create(target.certificateSha256)
        val mediaType = "application/json; charset=utf-8".toMediaType()
        val request = Request.Builder()
            .url(target.apiBaseUrl.trimEnd('/') + path)
            .header("Accept", "application/json")
            .header("Cache-Control", "no-store")
            .post(body.toString().toRequestBody(mediaType))
            .build()
        return client.newCall(request).await()
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
