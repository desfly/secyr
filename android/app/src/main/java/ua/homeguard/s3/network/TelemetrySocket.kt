package ua.homeguard.s3.network

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import org.json.JSONObject
import ua.homeguard.s3.model.SystemEventRecord
import ua.homeguard.s3.model.SystemSnapshot
import ua.homeguard.s3.model.SystemMode
import java.util.concurrent.TimeUnit
import java.util.concurrent.ScheduledThreadPoolExecutor
import java.util.concurrent.ScheduledFuture

enum class TelemetryConnectionState { IDLE, CONNECTING, CONNECTED, UNAUTHORIZED, OFFLINE }

class TelemetrySocket(private val telemetryTimeoutMs: Long = 15_000L) {
    companion object {
        private const val MAX_EVENT_HISTORY = 256
        private const val HEARTBEAT_SECONDS = 5L
        private val watchdog = ScheduledThreadPoolExecutor(1) { task ->
            Thread(task, "homeguard-telemetry-watchdog").apply { isDaemon = true }
        }.apply { removeOnCancelPolicy = true }
    }

    private val state = MutableStateFlow(SystemSnapshot())
    private val eventState = MutableStateFlow<List<SystemEventRecord>>(emptyList())
    private val liveEventState = MutableSharedFlow<SystemEventRecord>(extraBufferCapacity = 16)
    private val connectionState = MutableStateFlow(TelemetryConnectionState.IDLE)
    private var socket: WebSocket? = null
    private val receivedAtMs = MutableStateFlow(0L)
    private var lastFrameNanos = 0L
    private var watchdogTask: ScheduledFuture<*>? = null

    fun lastReceivedAtMs(): StateFlow<Long> = receivedAtMs.asStateFlow()
    private var fallbackSnapshot: SystemSnapshot? = null
    private var mqttMode: SystemMode? = null

    private fun fallbackState(): SystemSnapshot = fallbackSnapshot ?: SystemSnapshot(mode = mqttMode ?: SystemMode.DISARMED)

    /** Compact MQTT state carries no zone or sensor measurements. */
    @Synchronized
    fun acceptMqttMode(mode: SystemMode?) {
        mqttMode = mode
        if (connectionState.value != TelemetryConnectionState.CONNECTED) state.value = fallbackState()
    }

    fun snapshots(): Flow<SystemSnapshot> = state
    fun events(): Flow<List<SystemEventRecord>> = eventState
    fun liveEvents(): Flow<SystemEventRecord> = liveEventState
    fun connection(): StateFlow<TelemetryConnectionState> = connectionState.asStateFlow()

    fun seedEvents(events: List<SystemEventRecord>) {
        eventState.value = events.distinctBy { it.sequence }.sortedByDescending { it.sequence }.take(MAX_EVENT_HISTORY)
    }

    fun acceptExternalEvent(event: SystemEventRecord) {
        eventState.value = (listOf(event) + eventState.value)
            .distinctBy { it.sequence }
            .take(MAX_EVENT_HISTORY)
        liveEventState.tryEmit(event)
    }

    fun clearEvents() { eventState.value = emptyList() }

    /**
     * Feed an authenticated fallback transport snapshot into the same UI stream.
     * The newest fallback is cached even while WSS is healthy so a BLE/MQTT
     * snapshot can be promoted immediately if WSS drops between telemetry frames.
     */
    @Synchronized
    fun acceptFallbackSnapshot(snapshot: SystemSnapshot) {
        fallbackSnapshot = snapshot
        if (connectionState.value != TelemetryConnectionState.CONNECTED) {
            state.value = snapshot
        }
    }

    /** Drop fallback data when its authenticated transport/session is gone. */
    @Synchronized
    fun clearFallbackSnapshot() {
        fallbackSnapshot = null
        if (connectionState.value != TelemetryConnectionState.CONNECTED) {
            state.value = fallbackState()
        }
    }

    @Synchronized
    fun connect(url: String, token: String, certificateSha256: String = "") {
        disconnect()
        if (url.isBlank()) return
        connectionState.value = TelemetryConnectionState.CONNECTING
        val client = PinnedTlsClientFactory.create(certificateSha256, 0)
            .newBuilder()
            .pingInterval(HEARTBEAT_SECONDS, TimeUnit.SECONDS)
            .build()
        val request = Request.Builder().url(url).apply {
            if (token.isNotBlank()) header("Authorization", "Bearer $token")
        }.build()
        lastFrameNanos = System.nanoTime()
        socket = client.newWebSocket(request, object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                // An open socket is not evidence of device telemetry.
            }

            override fun onMessage(webSocket: WebSocket, text: String) = synchronized(this@TelemetrySocket) {
                if (socket !== webSocket) return@synchronized
                runCatching {
                    val json = JSONObject(text)
                    if (json.has("event")) {
                        val item = SystemEventRecord(
                            sequence = json.optLong("sequence", 0), timestampMs = json.optLong("timestampMs", 0),
                            event = json.optString("event", "unknown"), sourceId = json.optInt("sourceId", 0), value = json.optInt("value", 0),
                        )
                        eventState.value = (listOf(item) + eventState.value).distinctBy { it.sequence }.take(MAX_EVENT_HISTORY)
                        liveEventState.tryEmit(item)
                    } else if (json.optJSONArray("zones") != null) {
                        val snapshot = JsonParsers.snapshot(json)
                        state.value = snapshot
                        lastFrameNanos = System.nanoTime()
                        receivedAtMs.value = System.currentTimeMillis()
                        connectionState.value = TelemetryConnectionState.CONNECTED
                    }
                }
                Unit
            }

            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) = synchronized(this@TelemetrySocket) {
                if (socket === webSocket) {
                    socket = null
                    watchdogTask?.cancel(false)
                    watchdogTask = null
                    connectionState.value = if (response?.code == 401 || response?.code == 403) {
                        TelemetryConnectionState.UNAUTHORIZED
                    } else {
                        TelemetryConnectionState.OFFLINE
                    }
                    state.value = fallbackState()
                }
            }

            override fun onClosed(webSocket: WebSocket, code: Int, reason: String) = synchronized(this@TelemetrySocket) {
                if (socket === webSocket) {
                    socket = null
                    watchdogTask?.cancel(false)
                    watchdogTask = null
                    connectionState.value = if (code == 1008 || reason.contains("unauthor", true) || reason.contains("forbidden", true)) {
                        TelemetryConnectionState.UNAUTHORIZED
                    } else {
                        TelemetryConnectionState.OFFLINE
                    }
                    state.value = fallbackState()
                }
            }
        })
        watchdogTask = watchdog.scheduleAtFixedRate({
            synchronized(this) {
                val active = socket
                if (active != null && TimeUnit.NANOSECONDS.toMillis(System.nanoTime() - lastFrameNanos) >= telemetryTimeoutMs) {
                    socket = null
                    watchdogTask?.cancel(false)
                    watchdogTask = null
                    active.cancel()
                    state.value = fallbackState()
                    connectionState.value = TelemetryConnectionState.OFFLINE
                }
            }
        }, 100L, 100L, TimeUnit.MILLISECONDS)
    }

    @Synchronized
    fun disconnect() {
        val previous = socket
        socket = null
        watchdogTask?.cancel(false)
        watchdogTask = null
        previous?.cancel()
        receivedAtMs.value = 0L
        connectionState.value = TelemetryConnectionState.IDLE
        state.value = fallbackState()
    }
}
