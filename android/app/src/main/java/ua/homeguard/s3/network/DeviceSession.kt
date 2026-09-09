package ua.homeguard.s3.network

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.launch
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.storage.RegisteredDeviceStore
import ua.homeguard.s3.storage.SettingsStore

class DeviceSession(
    private val scope: CoroutineScope,
    private val endpointProvider: kotlinx.coroutines.flow.StateFlow<DeviceEndpoint>,
    private val settings: SettingsStore,
    private val telemetry: TelemetrySocket
) {
    companion object {
        private const val RECONNECT_DELAY_MS = 2_000L
    }

    private var job: Job? = null
    private var authorizationJob: Job? = null
    private var reconnectJob: Job? = null
    @Volatile private var activeTarget: SessionTarget? = null

    fun start() {
        if (job != null) return
        job = scope.launch {
            combine(endpointProvider, settings.settings) { endpoint, appSettings ->
                when (endpoint.path) {
                    ControlPath.CLOUD -> SessionTarget(endpoint, appSettings.apiToken, "")
                    ControlPath.OFFLINE -> SessionTarget(endpoint, "", "")
                    else -> {
                        // Prefer the login-scoped telemetry token while it is valid.
                        // A provisioned local API token is the reboot-safe fallback.
                        val sessionToken = appSettings.telemetryToken
                        val localApiToken = appSettings.apiToken
                        SessionTarget(
                            endpoint = endpoint,
                            token = sessionToken.ifBlank { localApiToken },
                            fallbackToken = if (
                                sessionToken.isNotBlank() &&
                                localApiToken.isNotBlank() &&
                                localApiToken != sessionToken
                            ) localApiToken else "",
                        )
                    }
                }
            }.distinctUntilChanged().collect { target: SessionTarget ->
                activeTarget = target
                reconnectJob?.cancel()
                reconnectJob = null
                connectTarget(target)
            }
        }

        authorizationJob = scope.launch {
            telemetry.connection().collect { state ->
                val deviceId = settings.settings.value.deviceId
                when (state) {
                    TelemetryConnectionState.UNAUTHORIZED -> {
                        reconnectJob?.cancel()
                        reconnectJob = null
                        val target = activeTarget
                        if (target != null && target.fallbackToken.isNotBlank()) {
                            // A login-scoped token becomes invalid after an ESP reboot.
                            // Drop it once and let the flow reconnect with the durable
                            // provisioned local API token instead of getting stuck at 401.
                            settings.update(settings.settings.value.copy(telemetryToken = ""))
                        } else {
                            RegisteredDeviceStore.markActiveAuthorization(deviceId, false)
                        }
                    }
                    TelemetryConnectionState.CONNECTED -> {
                        reconnectJob?.cancel()
                        reconnectJob = null
                        RegisteredDeviceStore.markActiveAuthorization(deviceId, true)
                    }
                    TelemetryConnectionState.OFFLINE -> scheduleReconnect()
                    else -> Unit
                }
            }
        }
    }

    fun stop() {
        activeTarget = null
        reconnectJob?.cancel()
        job?.cancel()
        authorizationJob?.cancel()
        reconnectJob = null
        job = null
        authorizationJob = null
        telemetry.disconnect()
    }

    private fun connectTarget(target: SessionTarget) {
        val endpoint = target.endpoint
        if (endpoint.path == ControlPath.OFFLINE || endpoint.websocketUrl.isBlank() || target.token.isBlank()) {
            telemetry.disconnect()
            return
        }
        val pin = if (endpoint.path == ControlPath.CLOUD) "" else endpoint.certificateSha256
        telemetry.connect(endpoint.websocketUrl, target.token, pin)
    }

    private fun scheduleReconnect() {
        if (!settings.settings.value.autoReconnect) return
        if (reconnectJob?.isActive == true) return
        val target = activeTarget ?: return
        if (target.endpoint.path == ControlPath.OFFLINE || target.endpoint.websocketUrl.isBlank() || target.token.isBlank()) return

        reconnectJob = scope.launch {
            delay(RECONNECT_DELAY_MS)
            if (activeTarget == target && telemetry.connection().value == TelemetryConnectionState.OFFLINE) {
                connectTarget(target)
            }
        }
    }

    private data class SessionTarget(
        val endpoint: DeviceEndpoint,
        val token: String,
        val fallbackToken: String,
    )
}
