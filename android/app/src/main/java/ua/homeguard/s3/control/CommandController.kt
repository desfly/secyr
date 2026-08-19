package ua.homeguard.s3.control

import kotlinx.coroutines.flow.StateFlow
import org.json.JSONObject
import ua.homeguard.s3.model.AccessLifecycleState
import ua.homeguard.s3.model.AccessSession
import ua.homeguard.s3.model.CommandReply
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceCommand
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.network.HttpDeviceApi
import ua.homeguard.s3.storage.SettingsStore
import java.util.concurrent.atomic.AtomicLong

class CommandController(
    private val endpoint: StateFlow<DeviceEndpoint>,
    private val settings: SettingsStore,
) {
    private val requestIds = AtomicLong(System.currentTimeMillis())
    @Volatile private var localHttpSessionToken: String = ""

    suspend fun accessState(): AccessLifecycleState {
        val target = localTarget()
        localHttpSessionToken = ""
        return createApi(target).accessState()
    }

    suspend fun bootstrapAdmin(id: String, name: String, pin: String) {
        val target = localTarget()
        localHttpSessionToken = ""
        createApi(target).bootstrapAdmin(id, name, pin)
    }

    suspend fun setupWifiScan(): JSONObject {
        val target = localTarget()
        localHttpSessionToken = ""
        return createApi(target).setupWifiScan()
    }

    suspend fun setupConfigureWifi(ssid: String, password: String): JSONObject {
        val target = localTarget()
        localHttpSessionToken = ""
        return createApi(target).setupConfigureWifi(ssid, password)
    }

    suspend fun login(actor: String, credential: String): AccessSession {
        val target = endpoint.value
        require(target.path != ControlPath.OFFLINE && target.apiBaseUrl.isNotBlank()) { "controller offline" }

        // Never reuse a bearer session across a fresh login attempt/device.
        localHttpSessionToken = ""
        val api = createApi(target)
        val session = api.login(actor, credential)
        if (target.path != ControlPath.CLOUD) {
            localHttpSessionToken = session.sessionToken
            val telemetryToken = api.telemetrySession(actor, credential)
            settings.update(settings.settings.value.copy(telemetryToken = telemetryToken))
        }
        return session
    }

    fun logout() {
        localHttpSessionToken = ""
    }

    suspend fun execute(type: CommandType, actor: String = "", credential: String = ""): CommandReply {
        val target = endpoint.value
        val appSettings = settings.settings.value
        if (target.path == ControlPath.OFFLINE || target.apiBaseUrl.isBlank()) return CommandReply(accepted = false, code = "offline")
        if (target.path == ControlPath.CLOUD && appSettings.apiToken.isBlank()) return CommandReply(accepted = false, code = "offline")
        if (target.path != ControlPath.CLOUD && actor.isBlank()) return CommandReply(accepted = false, code = "authorization_required")

        val api = createApi(target)
        val challenge = if (target.path == ControlPath.CLOUD && requiresChallenge(type)) api.challenge(type) else null
        val command = DeviceCommand(
            requestId = requestIds.incrementAndGet(),
            issuedAtMs = System.currentTimeMillis(),
            type = type,
            challenge = challenge,
            actor = actor.trim(),
            credential = credential,
        )
        return api.command(command)
    }

    private fun localTarget(): DeviceEndpoint {
        val target = endpoint.value
        require(target.path != ControlPath.OFFLINE && target.path != ControlPath.CLOUD && target.apiBaseUrl.isNotBlank()) {
            "local controller unavailable"
        }
        return target
    }

    private fun createApi(target: DeviceEndpoint): HttpDeviceApi {
        val localRuntime = target.path != ControlPath.CLOUD
        val pin = if (target.path == ControlPath.CLOUD) "" else target.certificateSha256
        return HttpDeviceApi(
            baseUrl = target.apiBaseUrl,
            tokenProvider = { if (localRuntime) localHttpSessionToken else settings.settings.value.apiToken },
            certificatePin = pin,
            runtimeV1 = localRuntime,
        )
    }

    private fun requiresChallenge(type: CommandType): Boolean = when (type) {
        CommandType.DISARM,
        CommandType.RESET_ALARM,
        CommandType.OPEN_VALVES,
        CommandType.ENTER_MAINTENANCE,
        -> true
        else -> false
    }
}
