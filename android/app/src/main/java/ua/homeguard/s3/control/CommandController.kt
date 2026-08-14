package ua.homeguard.s3.control

import kotlinx.coroutines.flow.StateFlow
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

    suspend fun login(actor: String, credential: String): AccessSession {
        val target = endpoint.value
        require(target.path != ControlPath.OFFLINE && target.apiBaseUrl.isNotBlank()) {
            "controller offline"
        }
        val api = createApi(target)
        val session = api.login(actor, credential)
        if (target.path != ControlPath.CLOUD) {
            val telemetryToken = api.telemetrySession(actor, credential)
            settings.update(settings.settings.value.copy(telemetryToken = telemetryToken))
        }
        return session
    }

    suspend fun execute(type: CommandType, actor: String = "", credential: String = ""): CommandReply {
        val target = endpoint.value
        val appSettings = settings.settings.value
        if (target.path == ControlPath.OFFLINE || target.apiBaseUrl.isBlank()) {
            return CommandReply(accepted = false, code = "offline")
        }

        if (target.path == ControlPath.CLOUD && appSettings.apiToken.isBlank()) {
            return CommandReply(accepted = false, code = "offline")
        }
        if (target.path != ControlPath.CLOUD && appSettings.apiToken.isBlank() &&
            (actor.isBlank() || credential.isBlank())) {
            return CommandReply(accepted = false, code = "authorization_required")
        }

        val api = createApi(target)
        // The active local HTTP runtime authenticates every command with actor + PIN
        // and does not expose the obsolete /api/challenge endpoint. Keep challenge
        // flow only for the legacy/cloud command transport.
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

    private fun createApi(target: DeviceEndpoint): HttpDeviceApi {
        val localRuntime = target.path != ControlPath.CLOUD
        val pin = if (target.path == ControlPath.CLOUD) "" else target.certificateSha256
        return HttpDeviceApi(
            baseUrl = target.apiBaseUrl,
            tokenProvider = { settings.settings.value.apiToken },
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
