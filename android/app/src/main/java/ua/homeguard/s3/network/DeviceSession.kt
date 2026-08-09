package ua.homeguard.s3.network

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.combine
import kotlinx.coroutines.flow.distinctUntilChanged
import kotlinx.coroutines.launch
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.storage.SettingsStore

class DeviceSession(
    private val scope: CoroutineScope,
    private val endpointProvider: kotlinx.coroutines.flow.StateFlow<DeviceEndpoint>,
    private val settings: SettingsStore,
    private val telemetry: TelemetrySocket,
    private val cloudState: CloudStateMqttClient,
) {
    private var job: Job? = null

    fun start() {
        if (job != null) return
        job = scope.launch {
            combine(endpointProvider, settings.settings) { endpoint, appSettings ->
                SessionTarget(endpoint, appSettings.apiToken)
            }.distinctUntilChanged().collect { target: SessionTarget ->
                val endpoint = target.endpoint
                when (endpoint.path) {
                    ControlPath.CLOUD -> {
                        telemetry.disconnect()
                        cloudState.connect(endpoint.deviceId)
                    }
                    ControlPath.LOCAL, ControlPath.LAST_KNOWN_LOCAL -> {
                        cloudState.disconnect()
                        if (endpoint.websocketUrl.isBlank() || target.token.isBlank()) {
                            telemetry.disconnect()
                        } else {
                            telemetry.connect(endpoint.websocketUrl, target.token, endpoint.certificateSha256)
                        }
                    }
                    ControlPath.OFFLINE -> {
                        cloudState.disconnect()
                        telemetry.disconnect()
                    }
                }
            }
        }
    }

    fun stop() {
        job?.cancel()
        job = null
        cloudState.disconnect()
        telemetry.disconnect()
    }

    private data class SessionTarget(val endpoint: DeviceEndpoint, val token: String)
}
