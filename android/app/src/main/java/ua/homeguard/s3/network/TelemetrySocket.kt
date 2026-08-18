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
import ua.homeguard.s3.model.sameEventIdentity
import java.util.concurrent.TimeUnit

enum class TelemetryConnectionState { IDLE, CONNECTING, CONNECTED, UNAUTHORIZED, OFFLINE }

internal fun mergeTelemetryEvent(
    history: List<SystemEventRecord>,
    event: SystemEventRecord,
    maxEvents: Int = 256,
): Pair<List<SystemEventRecord>, Boolean> {
    if (history.any { it.sameEventIdentity(event) }) return history to false
    return (listOf(event) + history).take(maxEvents.coerceAtLeast(0)) to true
}

class TelemetrySocket {
    companion object {
        private const val MAX_EVENT_HISTORY = 256
        private const val HEARTBEAT_SECONDS = 5L
    }

    private val state = MutableStateFlow(SystemSnapshot())
    private val eventState = MutableStateFlow<List<SystemEventRecord>>(emptyList())
    private val liveEventState = MutableSharedFlow<SystemEventRecord>(extraBufferCapacity = 16)
    private val connectionState = MutableStateFlow(TelemetryConnectionState.IDLE)
    private var socket: WebSocket? = null

    fun snapshots(): Flow<SystemSnapshot> = state
    fun events(): Flow<List<SystemEventRecord>> = eventState
    fun liveEvents(): Flow<SystemEventRecord> = liveEventState
    fun connection(): StateFlow<TelemetryConnectionState> = connectionState.asStateFlow()

    fun seedEvents(events: List<SystemEventRecord>) {
        val unique = mutableListOf<SystemEventRecord>()
        events
            .sortedWith(compareByDescending<SystemEventRecord> { it.timestampMs }.thenByDescending { it.sequence })
            .forEach { event ->
                if (unique.none { it.sameEventIdentity(event) }) unique += event
            }
        eventState.value = unique.take(MAX_EVENT_HISTORY)
    }

    fun clearEvents() { eventState.value = emptyList() }

    fun connect(url: String, token: String, certificateSha256: String = "", controllerId: String = "") {
        disconnect()
        if (url.isBlank()) return
        connectionState.value = TelemetryConnectionState.CONNECTING

        val newSocket = runCatching {
            val client = PinnedTlsClientFactory.create(certificateSha256, 0)
                .newBuilder()
                .pingInterval(HEARTBEAT_SECONDS, TimeUnit.SECONDS)
                .build()
            val request = Request.Builder().url(url).apply {
                if (token.isNotBlank()) header("Authorization", "Bearer $token")
            }.build()
            client.newWebSocket(request, listener(controllerId.trim()))
        }.getOrElse {
            state.value = SystemSnapshot()
            connectionState.value = TelemetryConnectionState.OFFLINE
            return
        }

        socket = newSocket
    }

    fun disconnect() {
        socket?.close(1000, "client disconnect")
        socket = null
        state.value = SystemSnapshot()
        connectionState.value = TelemetryConnectionState.IDLE
    }

    private fun listener(controllerId: String) = object : WebSocketListener() {
        override fun onOpen(webSocket: WebSocket, response: Response) {
            if (socket === webSocket) connectionState.value = TelemetryConnectionState.CONNECTED
        }

        override fun onMessage(webSocket: WebSocket, text: String) {
            // A close/reconnect is asynchronous. A frame from the previous
            // controller can still arrive after socket has already been replaced.
            // Never let stale telemetry overwrite the newly selected device state.
            if (socket !== webSocket) return
            connectionState.value = TelemetryConnectionState.CONNECTED
            runCatching {
                val json = JSONObject(text)
                if (json.has("event")) {
                    val item = SystemEventRecord(
                        sequence = json.optLong("sequence", 0),
                        timestampMs = json.optLong("timestampMs", 0),
                        event = json.optString("event", "unknown"),
                        sourceId = json.optInt("sourceId", 0),
                        value = json.optInt("value", 0),
                        controllerId = controllerId,
                    )
                    val (updatedEvents, added) = mergeTelemetryEvent(eventState.value, item, MAX_EVENT_HISTORY)
                    if (added) {
                        eventState.value = updatedEvents
                        liveEventState.tryEmit(item)
                    }
                } else {
                    state.value = JsonParsers.snapshot(json)
                }
            }
        }

        override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
            if (socket === webSocket) {
                socket = null
                state.value = SystemSnapshot()
                connectionState.value = if (response?.code == 401 || response?.code == 403) {
                    TelemetryConnectionState.UNAUTHORIZED
                } else {
                    TelemetryConnectionState.OFFLINE
                }
            }
        }

        override fun onClosed(webSocket: WebSocket, code: Int, reason: String) {
            if (socket === webSocket) {
                socket = null
                state.value = SystemSnapshot()
                connectionState.value = if (code == 1008 || reason.contains("unauthor", true) || reason.contains("forbidden", true)) {
                    TelemetryConnectionState.UNAUTHORIZED
                } else {
                    TelemetryConnectionState.OFFLINE
                }
            }
        }
    }
}
