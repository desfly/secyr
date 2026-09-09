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
    private var ticketRefreshJob: Job? = null
    @Volatile private var activeTarget: SessionTarget? = null

    fun start() {
        if (job != null) return
        job = scope.launch {
            combine(endpointProvider, settings.settings) { endpoint, appSettings ->
                when (endpoint.path) {
                    ControlPath.CLOUD -> SessionTarget(endpoint, appSettings.apiToken, false)
                    ControlPath.OFFLINE -> SessionTarget(endpoint, "", false)
                    else -> SessionTarget(
                        endpoint = endpoint,
                        token = appSettings.telemetryToken.ifBlank { appSettings.apiToken },
                        oneShotTicket = appSettings.telemetryToken.isNotBlank(),
                    )
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
                    TelemetryConnectionState.UNAUTHORIZED -> handleUnauthorized(deviceId)
                    TelemetryConnectionState.CONNECTED -> {
                        reconnectJob?.cancel()
                        reconnectJob = null
                        ticketRefreshJob?.cancel()
                        ticketRefreshJob = null
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
        ticketRefreshJob?.cancel()
        job?.cancel()
        authorizationJob?.cancel()
        reconnectJob = null
        ticketRefreshJob = null
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

    private fun handleUnauthorized(deviceId: String) {
        reconnectJob?.cancel()
        reconnectJob = null
        val target = activeTarget ?: return
        if (!isLocal(target.endpoint) || !target.oneShotTicket) {
            RegisteredDeviceStore.markActiveAuthorization(deviceId, false)
            return
        }
        refreshLocalTicket(deviceId, allowDurableFallback = true)
    }

    private fun scheduleReconnect() {
        if (!settings.settings.value.autoReconnect) return
        if (reconnectJob?.isActive == true || ticketRefreshJob?.isActive == true) return
        val target = activeTarget ?: return
        if (target.endpoint.path == ControlPath.OFFLINE || target.endpoint.websocketUrl.isBlank() || target.token.isBlank()) return

        reconnectJob = scope.launch {
            while (activeTarget == target && telemetry.connection().value == TelemetryConnectionState.OFFLINE) {
                delay(RECONNECT_DELAY_MS)
                if (activeTarget != target || telemetry.connection().value != TelemetryConnectionState.OFFLINE) return@launch

                if (isLocal(target.endpoint) && target.oneShotTicket) {
                    val refreshed = runCatching { LocalTelemetryTicketBroker.refresh() }
                    if (refreshed.isSuccess) return@launch
                    val message = refreshed.exceptionOrNull()?.message.orEmpty()
                    if (message.contains("401") || message.contains("403") || message.contains("authenticated local HTTP session unavailable")) {
                        refreshLocalTicket(settings.settings.value.deviceId, allowDurableFallback = true)
                        return@launch
                    }
                    // Network is probably still absent. Keep waiting and retry
                    // the authenticated HTTP ticket request instead of reusing
                    // an already consumed WebSocket ticket.
                    continue
                }

                connectTarget(target)
                return@launch
            }
        }
    }

    private fun refreshLocalTicket(deviceId: String, allowDurableFallback: Boolean) {
        if (ticketRefreshJob?.isActive == true) return
        ticketRefreshJob = scope.launch {
            val result = runCatching { LocalTelemetryTicketBroker.refresh() }
            if (result.isSuccess) return@launch

            val current = settings.settings.value
            if (allowDurableFallback && current.telemetryToken.isNotBlank() && current.apiToken.isNotBlank()) {
                // A provisioned local API token is a durable WebSocket fallback
                // when the login-scoped HTTP session disappeared after reboot.
                settings.update(current.copy(telemetryToken = ""))
            } else {
                RegisteredDeviceStore.markActiveAuthorization(deviceId, false)
            }
        }
    }

    private fun isLocal(endpoint: DeviceEndpoint): Boolean =
        endpoint.path != ControlPath.CLOUD && endpoint.path != ControlPath.OFFLINE

    private data class SessionTarget(
        val endpoint: DeviceEndpoint,
        val token: String,
        val oneShotTicket: Boolean,
    )
}
