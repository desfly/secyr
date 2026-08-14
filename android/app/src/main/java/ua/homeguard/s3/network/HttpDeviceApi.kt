package ua.homeguard.s3.network

import kotlinx.coroutines.suspendCancellableCoroutine
import okhttp3.Call
import okhttp3.Callback
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import okhttp3.Response
import org.json.JSONObject
import ua.homeguard.s3.model.*
import java.io.IOException
import kotlin.coroutines.resume
import kotlin.coroutines.resumeWithException

class HttpDeviceApi(
    baseUrl: String,
    private val tokenProvider: () -> String,
    certificatePin: String = "",
    private val runtimeV1: Boolean = false,
) : DeviceApi {
    private val root = baseUrl.trimEnd('/')
    private val client: OkHttpClient = PinnedTlsClientFactory.create(certificatePin)

    suspend fun login(actor: String, credential: String): AccessSession {
        val normalizedActor = actor.trim()
        require(normalizedActor.isNotEmpty()) { "User ID is required" }
        require(credential.length in 4..12 && credential.all(Char::isDigit)) { "PIN must contain 4-12 digits" }

        val json = execute(
            RuntimeApiContract.ACCESS_LOGIN_PATH,
            "POST",
            JSONObject().put("actor", normalizedActor).put("credential", credential),
        )
        if (!json.optBoolean("ok", false)) {
            throw IOException("Login rejected: ${json.optString("reason", "unknown")}")
        }

        val role = when (json.optString("role", "guest").lowercase()) {
            "admin" -> AccessRole.ADMIN
            "user" -> AccessRole.USER
            else -> AccessRole.GUEST
        }
        val raw = json.optJSONObject("capabilities") ?: JSONObject()
        return AccessSession(
            actor = json.optString("actor", normalizedActor),
            name = json.optString("name", normalizedActor),
            role = role,
            capabilities = AccessCapabilities(
                monitor = raw.optBoolean("monitor", true),
                armHome = raw.optBoolean("armHome", false),
                armAway = raw.optBoolean("armAway", false),
                disarm = raw.optBoolean("disarm", false),
                panic = raw.optBoolean("panic", false),
                valves = raw.optBoolean("valves", false),
                networkConfigure = raw.optBoolean("networkConfigure", false),
                accessManage = raw.optBoolean("accessManage", false),
                serviceInvalidate = raw.optBoolean("serviceInvalidate", false),
            ),
        )
    }

    suspend fun networkStatus(): JSONObject = execute(RuntimeApiContract.NETWORK_STATUS_PATH)

    suspend fun configureWifi(ssid: String, password: String, actor: String, credential: String): JSONObject {
        require(ssid.isNotBlank() && ssid.length <= 32) { "SSID is invalid" }
        require(password.isEmpty() || password.length in 8..64) { "Wi-Fi password is invalid" }
        val normalizedActor = actor.trim()
        require(normalizedActor.isNotEmpty()) { "User ID is required" }
        require(credential.length in 4..12 && credential.all(Char::isDigit)) { "PIN must contain 4-12 digits" }
        return execute(
            RuntimeApiContract.NETWORK_CONNECT_PATH,
            "POST",
            JSONObject()
                .put("ssid", ssid)
                .put("password", password)
                .put("actor", normalizedActor)
                .put("credential", credential),
        )
    }

    suspend fun telemetrySession(actor: String, credential: String): String {
        val json = execute(
            RuntimeApiContract.TELEMETRY_SESSION_PATH,
            "POST",
            JSONObject().put("actor", actor.trim()).put("credential", credential),
        )
        if (!json.optBoolean("ok", false)) {
            throw IOException("Telemetry login rejected: ${json.optString("reason", "unknown")}")
        }
        return json.optString("telemetryToken", "").also {
            if (it.length < 32) throw IOException("Telemetry session token missing")
        }
    }

    override suspend fun command(command: DeviceCommand): CommandReply =
        if (runtimeV1) runtimeCommand(command) else legacyCommand(command)

    private suspend fun legacyCommand(command: DeviceCommand): CommandReply {
        val body = JSONObject()
            .put("requestId", LegacyApiContract.requestId(command.requestId))
            .put("issuedAtMs", command.issuedAtMs.toString())
            .put("command", command.type.name.lowercase())
            .apply {
                command.challenge?.let { put("challenge", it) }
                if (command.actor.isNotBlank()) put("actor", command.actor)
                if (command.credential.isNotBlank()) put("credential", command.credential)
            }
        val json = execute(LegacyApiContract.COMMAND_PATH, "POST", body)
        return CommandReply(
            accepted = json.optBoolean("accepted", false),
            duplicate = json.optBoolean("duplicate", false),
            code = json.optString("code", "unknown"),
        )
    }

    private suspend fun runtimeCommand(command: DeviceCommand): CommandReply {
        val actor = command.actor.trim()
        val credential = command.credential
        if (actor.isBlank() || credential.isBlank()) {
            return CommandReply(false, code = "authorization_required")
        }

        return when (command.type) {
            CommandType.ARM_HOME -> runtimeSecurityCommand("security.arm_home", actor, credential)
            CommandType.ARM_AWAY -> runtimeSecurityCommand("security.arm_away", actor, credential)
            CommandType.DISARM -> runtimeSecurityCommand("security.disarm", actor, credential)
            CommandType.OPEN_VALVES -> runtimeValveCommand(true, actor, credential)
            CommandType.CLOSE_VALVES -> runtimeValveCommand(false, actor, credential)
            else -> CommandReply(false, code = "runtime_command_not_wired")
        }
    }

    private suspend fun runtimeSecurityCommand(command: String, actor: String, credential: String): CommandReply {
        val json = execute(
            RuntimeApiContract.SECURITY_COMMAND_PATH,
            "POST",
            JSONObject()
                .put("command", command)
                .put("actor", actor)
                .put("credential", credential),
        )
        val accepted = json.optBoolean("ok", false)
        return CommandReply(
            accepted = accepted,
            code = if (accepted) "accepted" else json.optString("reason", "rejected"),
        )
    }

    private suspend fun runtimeValveCommand(active: Boolean, actor: String, credential: String): CommandReply {
        for (outputId in 2..3) {
            val json = execute(
                RuntimeApiContract.OUTPUT_COMMAND_PATH,
                "POST",
                JSONObject()
                    .put("outputId", outputId)
                    .put("active", active)
                    .put("alarmActive", false)
                    .put("actor", actor)
                    .put("credential", credential),
            )
            if (!json.optBoolean("ok", false)) {
                return CommandReply(false, code = json.optString("reason", json.optString("status", "rejected")))
            }
        }
        return CommandReply(true, code = "accepted")
    }

    override suspend fun diagnostics(): Diagnostics =
        JsonParsers.diagnostics(execute(LegacyApiContract.HEALTH_PATH))

    override suspend fun snapshot(): SystemSnapshot =
        JsonParsers.snapshot(execute(LegacyApiContract.STATUS_PATH))

    suspend fun challenge(type: CommandType): Long {
        val json = execute(
            LegacyApiContract.CHALLENGE_PATH,
            "POST",
            JSONObject().put("command", type.name.lowercase()),
        )
        return json.getLong("challenge")
    }

    private suspend fun execute(path: String, method: String = "GET", json: JSONObject? = null): JSONObject {
        val mediaType = "application/json; charset=utf-8".toMediaType()
        val request = Request.Builder()
            .url(root + path)
            .header("Accept", "application/json")
            .apply {
                val token = tokenProvider()
                if (token.isNotBlank()) header("Authorization", "Bearer $token")
                if (method == "POST") post((json ?: JSONObject()).toString().toRequestBody(mediaType))
            }
            .build()
        return client.newCall(request).await().use { response ->
            val text = response.body?.string().orEmpty()
            if (!response.isSuccessful) throw IOException("HTTP ${response.code}: $text")
            if (text.isBlank()) JSONObject() else JSONObject(text)
        }
    }

    private suspend fun Call.await(): Response = suspendCancellableCoroutine { continuation ->
        continuation.invokeOnCancellation { cancel() }
        enqueue(object : Callback {
            override fun onFailure(call: Call, error: IOException) {
                if (continuation.isActive) continuation.resumeWithException(error)
            }

            override fun onResponse(call: Call, response: Response) {
                continuation.resume(response)
            }
        })
    }
}
