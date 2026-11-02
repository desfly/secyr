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
internal object MqttHealthPolicy {
    // Firmware publishes both heartbeat and state every 5 seconds. A heartbeat
    // that is missing for four periods cannot prove a working return channel.
    const val HEARTBEAT_TIMEOUT_MS = 20_000L
    const val STATE_TIMEOUT_MS = 15_000L

    fun ready(heartbeatAtMs: Long, stateAtMs: Long, availability: String, nowMs: Long): Boolean =
        heartbeatAtMs > 0L && stateAtMs > 0L &&
            nowMs >= heartbeatAtMs && nowMs - heartbeatAtMs <= HEARTBEAT_TIMEOUT_MS &&
            nowMs >= stateAtMs && nowMs - stateAtMs <= STATE_TIMEOUT_MS &&
            availability == "online"
}

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
                val heartbeat = client.lastHeartbeatAtMs().value
                val received = client.deviceState().value?.receivedAtMs ?: 0L
                if (received > 0L) lastSeenAtMs.value = received
                val linkReady = MqttHealthPolicy.ready(heartbeat, received, client.availability().value, System.currentTimeMillis())
                state.value = when (client.state().value) {
                    MqttRuntimeClient.State.CONNECTED -> if (linkReady) State.CONNECTED else State.OFFLINE
                    MqttRuntimeClient.State.CONNECTING -> State.CONNECTING
                    MqttRuntimeClient.State.OFFLINE,
                    MqttRuntimeClient.State.ERROR -> State.OFFLINE
                    MqttRuntimeClient.State.DISABLED -> State.DISABLED
                }
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
