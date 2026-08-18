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
    private val telemetry: TelemetrySocket,
    private val registeredDevices: RegisteredDeviceStore,
) {
    private var targetJob: Job? = null
    private var connectionJob: Job? = null
    private var reconnectJob: Job? = null
    private var currentTarget: SessionTarget? = null
    private var reconnectAttempt = 0

    fun start() {
        if (targetJob != null) return

        targetJob = scope.launch {
            combine(endpointProvider, settings.settings) { endpoint, appSettings ->
                val token = appSettings.telemetryToken.ifBlank { appSettings.apiToken }
                SessionTarget(endpoint, token, appSettings.autoReconnect)
            }.distinctUntilChanged().collect { target ->
                val previous = currentTarget
                currentTarget = target

                if (previous == null || !target.sameConnection(previous)) {
                    reconnectJob?.cancel()
                    reconnectJob = null
                    reconnectAttempt = 0
                    connect(target)
                } else if (!target.autoReconnect) {
                    reconnectJob?.cancel()
                    reconnectJob = null
                    reconnectAttempt = 0
                } else if (telemetry.connection().value == TelemetryConnectionState.OFFLINE) {
                    scheduleReconnect()
                }
            }
        }

        connectionJob = scope.launch {
            telemetry.connection().collect { state ->
                val deviceId = settings.settings.value.deviceId
                when (state) {
                    TelemetryConnectionState.CONNECTED -> {
                        reconnectAttempt = 0
                        reconnectJob?.cancel()
                        reconnectJob = null
                        registeredDevices.markAuthorization(deviceId, true)
                    }
                    TelemetryConnectionState.UNAUTHORIZED -> {
                        reconnectJob?.cancel()
                        reconnectJob = null
                        registeredDevices.markAuthorization(deviceId, false)
                    }
                    TelemetryConnectionState.OFFLINE -> scheduleReconnect()
                    TelemetryConnectionState.IDLE,
                    TelemetryConnectionState.CONNECTING -> Unit
                }
            }
        }
    }

    fun stop() {
        targetJob?.cancel()
        connectionJob?.cancel()
        reconnectJob?.cancel()
        targetJob = null
        connectionJob = null
        reconnectJob = null
        currentTarget = null
        reconnectAttempt = 0
        telemetry.disconnect()
    }

    private fun connect(target: SessionTarget) {
        val endpoint = target.endpoint
        if (!target.isConnectable()) {
            telemetry.disconnect()
            return
        }
        val pin = if (endpoint.path == ControlPath.CLOUD) "" else endpoint.certificateSha256
        telemetry.connect(endpoint.websocketUrl, target.token, pin)
    }

    private fun scheduleReconnect() {
        if (reconnectJob?.isActive == true) return
        val target = currentTarget?.takeIf(SessionTarget::canReconnect) ?: return
        val attempt = reconnectAttempt++

        reconnectJob = scope.launch {
            delay(reconnectDelayMs(attempt))
            if (currentTarget == target && telemetry.connection().value == TelemetryConnectionState.OFFLINE) {
                connect(target)
            }
        }
    }

    private data class SessionTarget(
        val endpoint: DeviceEndpoint,
        val token: String,
        val autoReconnect: Boolean,
    ) {
        fun isConnectable(): Boolean =
            endpoint.path != ControlPath.OFFLINE && endpoint.websocketUrl.isNotBlank() && token.isNotBlank()

        fun canReconnect(): Boolean = reconnectAllowed(autoReconnect, endpoint, token)

        fun sameConnection(other: SessionTarget): Boolean = endpoint == other.endpoint && token == other.token
    }
}

internal fun reconnectAllowed(autoReconnect: Boolean, endpoint: DeviceEndpoint, token: String): Boolean =
    autoReconnect && endpoint.path != ControlPath.OFFLINE && endpoint.websocketUrl.isNotBlank() && token.isNotBlank()

internal fun reconnectDelayMs(attempt: Int): Long = when (attempt.coerceAtLeast(0)) {
    0 -> 2_000L
    1 -> 5_000L
    2 -> 10_000L
    3 -> 20_000L
    else -> 30_000L
}
