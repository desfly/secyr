package ua.homeguard.s3.network

import kotlinx.coroutines.flow.Flow
import kotlinx.coroutines.flow.MutableStateFlow
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import org.json.JSONObject
import ua.homeguard.s3.model.SystemSnapshot

class TelemetrySocket {
    private val state = MutableStateFlow(SystemSnapshot())
    private var socket: WebSocket? = null
    fun snapshots(): Flow<SystemSnapshot> = state

    fun connect(url: String, token: String, certificateSha256: String = "") {
        disconnect()
        if (url.isBlank()) return
        val client = PinnedTlsClientFactory.create(certificateSha256, 0)
        val request = Request.Builder().url(url).apply {
            if (token.isNotBlank()) header("Authorization", "Bearer $token")
        }.build()
        socket = client.newWebSocket(request, object : WebSocketListener() {
            override fun onMessage(webSocket: WebSocket, text: String) {
                runCatching { JsonParsers.snapshot(JSONObject(text)) }.onSuccess { state.value = it }
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
}
