package ua.homeguard.s3.network

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import org.json.JSONObject
import ua.homeguard.s3.model.HealthState
import ua.homeguard.s3.model.OutputStatus
import ua.homeguard.s3.model.SensorStatus
import ua.homeguard.s3.model.SystemEventRecord
import ua.homeguard.s3.model.SystemMode
import ua.homeguard.s3.model.SystemSnapshot
import ua.homeguard.s3.model.ZoneStatus

class TelemetrySocket {
    companion object { private const val MAX_EVENT_HISTORY = 256 }

    private val state = MutableStateFlow(SystemSnapshot())
    private val eventState = MutableStateFlow<List<SystemEventRecord>>(emptyList())
    private val liveEventState = MutableSharedFlow<SystemEventRecord>(extraBufferCapacity = 16)
    private var socket: WebSocket? = null

    fun snapshots(): Flow<SystemSnapshot> = state
    fun events(): Flow<List<SystemEventRecord>> = eventState
    fun liveEvents(): Flow<SystemEventRecord> = liveEventState

    fun seedEvents(events: List<SystemEventRecord>) {
        eventState.value = events.distinctBy { it.sequence }.sortedByDescending { it.sequence }.take(MAX_EVENT_HISTORY)
    }

    fun clearEvents() {
        eventState.value = emptyList()
    }

    fun acceptCloudSensorStatus(text: String): Boolean = runCatching {
        val json = JSONObject(text)
        if (!json.optBoolean("ok") || json.optString("code") != "sensors") return false
        val sensorsJson = json.optJSONArray("sensors") ?: return false
        val sensors = buildList {
            for (i in 0 until sensorsJson.length()) {
                val item = sensorsJson.optJSONObject(i) ?: continue
                val online = item.optBoolean("online", false)
                val battery = item.optInt("battery_percent", -1)
                val rssi = item.optInt("rssi_dbm", 0)
                add(
                    SensorStatus(
                        index = item.optInt("id", i + 1),
                        name = "Sensor ${item.optInt("id", i + 1)} · ${item.optString("type", "sensor")}",
                        state = if (online) "online" else "offline",
                        value = buildString {
                            if (battery >= 0) append("$battery%")
                            if (rssi != 0) {
                                if (isNotEmpty()) append(" · ")
                                append("$rssi dBm")
                            }
                        },
                    )
                )
            }
        }
        // Privacy boundary: sensor-only updates never alter arm mode, zones, outputs or events.
        state.value = state.value.copy(sensors = sensors)
        true
    }.getOrDefault(false)

    /** Accepts the retained MQTT state schema published by Build-0060 firmware. */
    fun acceptCloudState(text: String): Boolean = runCatching {
        val json = JSONObject(text)
        if (json.optInt("schema", 0) != 1) return false

        val zonesJson = json.optJSONArray("zones")
        val zones = buildList {
            if (zonesJson != null) for (i in 0 until zonesJson.length()) {
                val item = zonesJson.optJSONObject(i) ?: continue
                add(
                    ZoneStatus(
                        index = item.optInt("id", i + 1),
                        name = item.optString("name", "Zone ${i + 1}"),
                        state = item.optString("state", "unknown"),
                        enabled = item.optBoolean("enabled", true),
                    )
                )
            }
        }

        val sensorsJson = json.optJSONArray("sensors")
        val sensors = buildList {
            if (sensorsJson != null) for (i in 0 until sensorsJson.length()) {
                val item = sensorsJson.optJSONObject(i) ?: continue
                add(
                    SensorStatus(
                        index = item.optInt("id", i + 1),
                        name = item.optString("name", "Sensor ${i + 1}"),
                        state = item.optString("state", "unknown"),
                        value = if (item.has("value")) item.opt("value")?.toString().orEmpty() else "",
                    )
                )
            }
        }

        val outputsJson = json.optJSONArray("outputs")
        val outputs = buildList {
            if (outputsJson != null) for (i in 0 until outputsJson.length()) {
                val item = outputsJson.optJSONObject(i) ?: continue
                add(OutputStatus(index = item.optInt("id", i + 1), active = item.optBoolean("active", false)))
            }
        }

        state.value = state.value.copy(
            sequence = json.optLong("sequence", state.value.sequence),
            uptimeMs = json.optLong("timestamp_ms", state.value.uptimeMs),
            mode = cloudMode(json.optString("arm_state", "disarmed")),
            health = HealthState.OK,
            zones = zones,
            sensors = sensors,
            outputs = outputs,
        )
        true
    }.getOrDefault(false)

    fun connect(url: String, token: String, certificateSha256: String = "") {
        disconnect()
        if (url.isBlank()) return
        val client = PinnedTlsClientFactory.create(certificateSha256, 0)
        val request = Request.Builder().url(url).apply {
            if (token.isNotBlank()) header("Authorization", "Bearer $token")
        }.build()
        socket = client.newWebSocket(request, object : WebSocketListener() {
            override fun onMessage(webSocket: WebSocket, text: String) {
                runCatching {
                    val json = JSONObject(text)
                    if (json.has("event")) {
                        val item = SystemEventRecord(
                            sequence = json.optLong("sequence", 0),
                            timestampMs = json.optLong("timestampMs", 0),
                            event = json.optString("event", "unknown"),
                            sourceId = json.optInt("sourceId", 0),
                            value = json.optInt("value", 0),
                        )
                        eventState.value = (listOf(item) + eventState.value)
                            .distinctBy { it.sequence }
                            .take(MAX_EVENT_HISTORY)
                        liveEventState.tryEmit(item)
                    } else {
                        state.value = JsonParsers.snapshot(json)
                    }
                }
            }
            override fun onFailure(webSocket: WebSocket, t: Throwable, response: Response?) {
                if (socket === webSocket) socket = null
            }
            override fun onClosed(webSocket: WebSocket, code: Int, reason: String) {
                if (socket === webSocket) socket = null
            }
        })
    }

    fun disconnect() {
        socket?.close(1000, "client disconnect")
        socket = null
    }

    private fun cloudMode(value: String): SystemMode = when (value.lowercase()) {
        "armed_home", "arm_home", "home" -> SystemMode.ARMED_HOME
        "armed_away", "arm_away", "away", "armed" -> SystemMode.ARMED_AWAY
        "alarm" -> SystemMode.ALARM
        "maintenance" -> SystemMode.MAINTENANCE
        else -> SystemMode.DISARMED
    }
}
