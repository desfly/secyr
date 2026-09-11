package ua.homeguard.s3.network.cloud

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.network.TelemetryConnectionState
import ua.homeguard.s3.network.TelemetrySocket

/**
 * Independent runtime health model for the legacy HTTP/WSS cloud path.
 *
 * This class intentionally has no dependency on MQTT. It observes only the
 * cloud endpoint selected by DeviceEndpointResolver and the cloud WebSocket
 * connection state managed by DeviceSession/TelemetrySocket.
 */
class CloudRuntime(
    private val scope: CoroutineScope,
    private val endpointProvider: StateFlow<DeviceEndpoint>,
    private val telemetry: TelemetrySocket,
) {
    enum class State { DISABLED, CONNECTING, CONNECTED, OFFLINE }

    private val state = MutableStateFlow(State.DISABLED)
    private val lastSeenAtMs = MutableStateFlow(0L)
    private var job: Job? = null

    fun state(): StateFlow<State> = state
    fun lastSeenAtMs(): StateFlow<Long> = lastSeenAtMs

    fun start() {
        if (job != null) return
        job = scope.launch {
            while (isActive) {
                val endpoint = endpointProvider.value
                if (endpoint.path != ControlPath.CLOUD || endpoint.websocketUrl.isBlank()) {
                    state.value = State.DISABLED
                } else {
                    when (telemetry.connection().value) {
                        TelemetryConnectionState.CONNECTED -> {
                            state.value = State.CONNECTED
                            lastSeenAtMs.value = System.currentTimeMillis()
                        }
                        TelemetryConnectionState.CONNECTING -> state.value = State.CONNECTING
                        else -> state.value = State.OFFLINE
                    }
                }
                delay(500L)
            }
        }
    }

    fun stop() {
        job?.cancel()
        job = null
        state.value = State.DISABLED
        lastSeenAtMs.value = 0L
    }
}
