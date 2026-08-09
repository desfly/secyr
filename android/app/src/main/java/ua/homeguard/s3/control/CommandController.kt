package ua.homeguard.s3.control

import kotlinx.coroutines.flow.StateFlow
import ua.homeguard.s3.model.CommandReply
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceCommand
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.network.CloudStateMqttClient
import ua.homeguard.s3.network.HttpDeviceApi
import ua.homeguard.s3.storage.SettingsStore
import java.util.concurrent.atomic.AtomicLong

class CommandController(
    private val endpoint: StateFlow<DeviceEndpoint>,
    private val settings: SettingsStore,
    private val cloud: CloudStateMqttClient,
) {
    private val requestIds = AtomicLong(System.currentTimeMillis())

    suspend fun execute(type: CommandType, actor: String = "", credential: String = ""): CommandReply {
        val target = endpoint.value
        if (target.path == ControlPath.CLOUD) {
            val command = cloudCommand(type) ?: return CommandReply(false, code = "cloud_unsupported")
            return cloud.executeCommand(command, actor.trim(), credential)
        }

        val appSettings = settings.settings.value
        if (target.path == ControlPath.OFFLINE || target.apiBaseUrl.isBlank() || appSettings.apiToken.isBlank()) {
            return CommandReply(accepted = false, code = "offline")
        }

        val api = HttpDeviceApi(
            baseUrl = target.apiBaseUrl,
            tokenProvider = { settings.settings.value.apiToken },
            certificatePin = target.certificateSha256,
        )

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

    private fun cloudCommand(type: CommandType): String? = when (type) {
        CommandType.ARM_HOME -> "security.arm_home"
        CommandType.ARM_AWAY -> "security.arm_away"
        CommandType.DISARM -> "security.disarm"
        CommandType.OPEN_VALVES -> "valve.open"
        CommandType.CLOSE_VALVES -> "valve.close"
        else -> null
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
