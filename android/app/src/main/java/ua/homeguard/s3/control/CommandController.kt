package ua.homeguard.s3.control

import kotlinx.coroutines.async
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.coroutineScope
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.first
import kotlinx.coroutines.selects.select
import ua.homeguard.s3.model.AccessSession
import ua.homeguard.s3.model.CommandReply
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceCommand
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.network.HttpDeviceApi
import ua.homeguard.s3.storage.SettingsStore
import java.util.concurrent.atomic.AtomicLong

internal fun sameSelectedController(expectedDeviceId: String, selectedDeviceId: String): Boolean =
    expectedDeviceId.isNotBlank() && expectedDeviceId.trim().equals(selectedDeviceId.trim(), ignoreCase = true)

class CommandController(
    private val endpoint: StateFlow<DeviceEndpoint>,
    private val settings: SettingsStore,
) {
    private val requestIds = AtomicLong(System.currentTimeMillis())

    suspend fun login(actor: String, credential: String): AccessSession {
        val target = endpoint.value
        val appSettings = settings.settings.value
        require(target.path != ControlPath.OFFLINE && target.apiBaseUrl.isNotBlank()) {
            "controller offline"
        }
        requireStillSelected(target)
        val api = createApi(target, appSettings.apiToken)
        val session = runWhileSelected(target) { api.login(actor, credential) }

        if (target.path != ControlPath.CLOUD) {
            val telemetryToken = runWhileSelected(target) { api.telemetrySession(actor, credential) }
            requireStillSelected(target)
            settings.update(settings.settings.value.copy(telemetryToken = telemetryToken))
        }
        requireStillSelected(target)
        return session.copy(controllerId = target.deviceId)
    }

    suspend fun execute(type: CommandType, actor: String = "", credential: String = ""): CommandReply {
        val target = endpoint.value
        val appSettings = settings.settings.value
        if (!sameSelectedController(target.deviceId, appSettings.deviceId)) {
            return CommandReply(accepted = false, code = "controller_changed")
        }
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

        // Freeze the token with the target. A device switch must never make an in-flight
        // request to controller A read controller B's token from SettingsStore.
        val api = createApi(target, appSettings.apiToken)
        return runCatching {
            val challenge = if (target.path == ControlPath.CLOUD && requiresChallenge(type)) {
                runWhileSelected(target) { api.challenge(type) }
            } else {
                null
            }
            val command = DeviceCommand(
                requestId = requestIds.incrementAndGet(),
                issuedAtMs = System.currentTimeMillis(),
                type = type,
                challenge = challenge,
                actor = actor.trim(),
                credential = credential,
            )
            runWhileSelected(target) { api.command(command) }
        }.getOrElse { error ->
            if (error is ControllerChangedException) {
                CommandReply(accepted = false, code = "controller_changed")
            } else {
                throw error
            }
        }
    }

    private suspend fun <T> runWhileSelected(target: DeviceEndpoint, block: suspend () -> T): T = coroutineScope {
        requireStillSelected(target)
        val request = async { block() }
        val selectionChanged = async {
            settings.settings.first { current -> !sameSelectedController(target.deviceId, current.deviceId) }
        }
        try {
            select {
                request.onAwait { result ->
                    selectionChanged.cancel()
                    result
                }
                selectionChanged.onAwait {
                    request.cancelAndJoin()
                    throw ControllerChangedException()
                }
            }
        } finally {
            selectionChanged.cancel()
        }
    }

    private fun requireStillSelected(target: DeviceEndpoint) {
        if (!sameSelectedController(target.deviceId, settings.settings.value.deviceId)) {
            throw ControllerChangedException()
        }
    }

    private fun createApi(target: DeviceEndpoint, apiToken: String): HttpDeviceApi {
        val localRuntime = target.path != ControlPath.CLOUD
        val pin = if (target.path == ControlPath.CLOUD) "" else target.certificateSha256
        return HttpDeviceApi(
            baseUrl = target.apiBaseUrl,
            tokenProvider = { apiToken },
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

    private class ControllerChangedException : IllegalStateException("controller changed during request")
}
