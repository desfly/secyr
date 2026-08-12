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
        return requireOnlineApi().login(actor, credential)
    }

    suspend fun execute(type: CommandType, actor: String = "", credential: String = ""): CommandReply {
        val target = endpoint.value
        val appSettings = settings.settings.value
        if (target.path == ControlPath.OFFLINE || target.apiBaseUrl.isBlank() || appSettings.apiToken.isBlank()) {
            return CommandReply(accepted = false, code = "offline")
        }

        val api = createApi(target)
        val challenge = if (requiresChallenge(type)) api.challenge(type) else null
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

    private fun requireOnlineApi(): HttpDeviceApi {
        val target = endpoint.value
        val appSettings = settings.settings.value
        require(target.path != ControlPath.OFFLINE && target.apiBaseUrl.isNotBlank() && appSettings.apiToken.isNotBlank()) {
            "controller offline"
        }
        return createApi(target)
    }

    private fun createApi(target: DeviceEndpoint): HttpDeviceApi {
        val pin = if (target.path == ControlPath.CLOUD) "" else target.certificateSha256
        return HttpDeviceApi(
            baseUrl = target.apiBaseUrl,
            tokenProvider = { settings.settings.value.apiToken },
            certificatePin = pin,
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
