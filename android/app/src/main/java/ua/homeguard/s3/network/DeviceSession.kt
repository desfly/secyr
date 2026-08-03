package ua.homeguard.s3.network

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.collect
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.launch
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.storage.SettingsStore

class DeviceSession(
    private val scope: CoroutineScope,
    private val endpointProvider: kotlinx.coroutines.flow.StateFlow<DeviceEndpoint>,
    private val settings: SettingsStore,
    private val telemetry: TelemetrySocket
) {
    private var job: Job? = null

    fun start() {
        if (job != null) return
        job = scope.launch {
            combine(endpointProvider, settings.settings) { endpoint, appSettings ->
                SessionTarget(endpoint, appSettings.apiToken)
            }.distinctUntilChanged().collect { target: SessionTarget ->
                val endpoint = target.endpoint
                if (endpoint.path == ControlPath.OFFLINE || endpoint.websocketUrl.isBlank() || target.token.isBlank()) {
                    telemetry.disconnect()
                } else {
                    val pin = if (endpoint.path == ControlPath.CLOUD) "" else endpoint.certificateSha256
                    telemetry.connect(endpoint.websocketUrl, target.token, pin)
                }
            }
        }
    }

    fun stop() {
        job?.cancel()
        job = null
        telemetry.disconnect()
    }

    private data class SessionTarget(val endpoint: DeviceEndpoint, val token: String)
}
