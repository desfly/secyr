package ua.homeguard.s3.network.mqtt

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch

/**
 * Independent MQTT runtime health wrapper.
 *
 * MQTT is treated as its own Internet transport and does not depend on the
 * legacy HTTP/WSS cloud path.
 */
class MqttRuntime(
    private val scope: CoroutineScope,
    private val client: MqttRuntimeClient,
) {
    enum class State { DISABLED, CONNECTING, CONNECTED, OFFLINE }

    private val state = MutableStateFlow(State.DISABLED)
    private val lastSeenAtMs = MutableStateFlow(0L)
    private var job: Job? = null

    fun state(): StateFlow<State> = state
    fun lastSeenAtMs(): StateFlow<Long> = lastSeenAtMs

    fun start(config: MqttRuntimeClient.Config) {
        client.start(config)
        if (job != null) return
        job = scope.launch {
            while (isActive) {
                state.value = when (client.state().value) {
                    MqttRuntimeClient.State.CONNECTED -> State.CONNECTED
                    MqttRuntimeClient.State.CONNECTING -> State.CONNECTING
                    MqttRuntimeClient.State.OFFLINE,
                    MqttRuntimeClient.State.ERROR -> State.OFFLINE
                    MqttRuntimeClient.State.DISABLED -> State.DISABLED
                }
                val heartbeat = client.lastHeartbeatAtMs().value
                if (heartbeat > 0L) lastSeenAtMs.value = heartbeat
                delay(500L)
            }
        }
    }

    fun stop() {
        job?.cancel()
        job = null
        client.stop()
        state.value = State.DISABLED
        lastSeenAtMs.value = 0L
    }
}
