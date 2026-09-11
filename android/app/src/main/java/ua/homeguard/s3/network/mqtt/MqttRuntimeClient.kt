package ua.homeguard.s3.network.mqtt

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.isActive
import kotlinx.coroutines.launch
import org.json.JSONObject
import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.io.EOFException
import java.net.Socket
import java.net.URI
import java.nio.charset.StandardCharsets
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.atomic.AtomicInteger
import javax.net.ssl.SSLSocketFactory

/** Minimal MQTT 3.1.1 runtime used by HomeGuard Android. */
class MqttRuntimeClient(private val scope: CoroutineScope) {
    enum class State { DISABLED, CONNECTING, CONNECTED, OFFLINE, ERROR }

    data class Config(
        val brokerUri: String,
        val username: String = "",
        val password: String = "",
        val deviceId: String,
        val clientId: String = "android-${System.currentTimeMillis()}",
    )

    private val state = MutableStateFlow(State.DISABLED)
    private val availability = MutableStateFlow("unknown")
    private val lastHeartbeatAtMs = MutableStateFlow(0L)
    private val events = MutableSharedFlow<JSONObject>(extraBufferCapacity = 64)
    private val responses = MutableSharedFlow<JSONObject>(extraBufferCapacity = 32)
    private val pendingResponses = ConcurrentHashMap<String, kotlinx.coroutines.CompletableDeferred<JSONObject>>()
    private val packetIds = AtomicInteger(1)

    @Volatile private var config: Config? = null
    @Volatile private var socket: Socket? = null
    @Volatile private var output: BufferedOutputStream? = null
    private var worker: Job? = null

    fun state(): StateFlow<State> = state
    fun availability(): StateFlow<String> = availability
    fun lastHeartbeatAtMs(): StateFlow<Long> = lastHeartbeatAtMs
    fun events(): SharedFlow<JSONObject> = events
    fun responses(): SharedFlow<JSONObject> = responses

    fun start(next: Config) {
        require(next.deviceId.isNotBlank()) { "MQTT device id is empty" }
        require(next.brokerUri.isNotBlank()) { "MQTT broker URI is empty" }
        if (config == next && worker?.isActive == true) return
        stop()
        config = next
        worker = scope.launch(Dispatchers.IO) { connectionLoop(next) }
    }

    fun stop() {
        worker?.cancel()
        worker = null
        runCatching { socket?.close() }
        socket = null
        output = null
        pendingResponses.values.forEach { it.cancel() }
        pendingResponses.clear()
        config = null
        availability.value = "unknown"
        lastHeartbeatAtMs.value = 0L
        state.value = State.DISABLED
    }

    suspend fun command(
        requestId: String,
        actor: String,
        credential: String,
        command: String,
        timeoutMs: Long = 8_000L,
    ): JSONObject {
        require(requestId.isNotBlank()) { "MQTT request id is empty" }
        require(actor.isNotBlank()) { "MQTT actor is empty" }
        require(credential.isNotBlank()) { "MQTT credential is empty" }
        require(command.isNotBlank()) { "MQTT command is empty" }
        val cfg = config ?: error("MQTT disabled")
        check(state.value == State.CONNECTED) { "MQTT offline" }

        val waiter = kotlinx.coroutines.CompletableDeferred<JSONObject>()
        pendingResponses[requestId] = waiter
        try {
            val body = JSONObject()
                .put("request_id", requestId)
                .put("actor", actor)
                .put("credential", credential)
                .put("command", command)
                .toString()
            publish(topic(cfg, "commands"), body, qos = 1, retain = false)
            return kotlinx.coroutines.withTimeout(timeoutMs) { waiter.await() }
        } finally {
            pendingResponses.remove(requestId)
        }
    }

    private suspend fun connectionLoop(cfg: Config) {
        var backoffMs = 1_000L
        while (scope.isActive && config == cfg) {
            try {
                state.value = State.CONNECTING
                open(cfg)
                state.value = State.CONNECTED
                availability.value = "unknown"
                backoffMs = 1_000L
                readLoop(cfg)
            } catch (cancel: kotlinx.coroutines.CancellationException) {
                throw cancel
            } catch (_: Throwable) {
                state.value = State.OFFLINE
            } finally {
                runCatching { socket?.close() }
                socket = null
                output = null
            }
            if (config != cfg) break
            delay(backoffMs)
            backoffMs = (backoffMs * 2).coerceAtMost(15_000L)
        }
    }

    private fun open(cfg: Config) {
        val uri = normalizeUri(cfg.brokerUri)
        val secure = uri.scheme.equals("mqtts", true) || uri.scheme.equals("ssl", true)
        val port = if (uri.port > 0) uri.port else if (secure) 8883 else 1883
        val host = requireNotNull(uri.host) { "MQTT host missing" }
        val raw = if (secure) SSLSocketFactory.getDefault().createSocket(host, port) else Socket(host, port)
        raw.tcpNoDelay = true
        raw.soTimeout = 90_000
        socket = raw
        val input = BufferedInputStream(raw.getInputStream())
        output = BufferedOutputStream(raw.getOutputStream())

        writeConnect(cfg)
        val packet = readPacket(input)
        check(packet.type == 2 && packet.payload.size >= 2 && packet.payload[1].toInt() == 0) { "MQTT CONNACK rejected" }
        subscribe(input, topic(cfg, "availability"))
        subscribe(input, topic(cfg, "heartbeat"))
        subscribe(input, topic(cfg, "events"))
        subscribe(input, topic(cfg, "responses"))
    }

    private fun readLoop(cfg: Config) {
        val input = BufferedInputStream(checkNotNull(socket).getInputStream())
        var lastPingAt = System.currentTimeMillis()
        while (worker?.isActive == true && config == cfg) {
            if (System.currentTimeMillis() - lastPingAt >= 30_000L) {
                writeRaw(byteArrayOf(0xC0.toByte(), 0x00))
                lastPingAt = System.currentTimeMillis()
            }
            val packet = readPacket(input)
            when (packet.type) {
                3 -> handlePublish(cfg, packet.flags, packet.payload)
                13 -> Unit
                else -> Unit
            }
        }
    }

    private fun handlePublish(cfg: Config, flags: Int, payload: ByteArray) {
        if (payload.size < 2) return
        var offset = 0
        val topicLength = ((payload[offset].toInt() and 0xff) shl 8) or (payload[offset + 1].toInt() and 0xff)
        offset += 2
        if (offset + topicLength > payload.size) return
        val incomingTopic = String(payload, offset, topicLength, StandardCharsets.UTF_8)
        offset += topicLength
        val qos = (flags shr 1) and 0x03
        var packetId = 0
        if (qos > 0) {
            if (offset + 2 > payload.size) return
            packetId = ((payload[offset].toInt() and 0xff) shl 8) or (payload[offset + 1].toInt() and 0xff)
            offset += 2
        }
        val body = String(payload, offset, payload.size - offset, StandardCharsets.UTF_8)
        if (qos == 1 && packetId != 0) writePubAck(packetId)

        when (incomingTopic) {
            topic(cfg, "availability") -> availability.value = body.trim()
            topic(cfg, "heartbeat") -> {
                lastHeartbeatAtMs.value = System.currentTimeMillis()
                availability.value = "online"
            }
            topic(cfg, "events") -> runCatching { JSONObject(body) }.getOrNull()?.let(events::tryEmit)
            topic(cfg, "responses") -> {
                val json = runCatching { JSONObject(body) }.getOrNull() ?: return
                responses.tryEmit(json)
                val id = json.optString("requestId").ifBlank { json.optString("request_id") }
                if (id.isNotBlank()) pendingResponses[id]?.complete(json)
            }
        }
    }

    @Synchronized
    private fun publish(topic: String, payload: String, qos: Int, retain: Boolean) {
        val body = payload.toByteArray(StandardCharsets.UTF_8)
        val topicBytes = topic.toByteArray(StandardCharsets.UTF_8)
        val variable = ArrayList<Byte>(topicBytes.size + body.size + 8)
        variable += ((topicBytes.size shr 8) and 0xff).toByte()
        variable += (topicBytes.size and 0xff).toByte()
        topicBytes.forEach(variable::add)
        if (qos > 0) {
            val id = nextPacketId()
            variable += ((id shr 8) and 0xff).toByte()
            variable += (id and 0xff).toByte()
        }
        body.forEach(variable::add)
        var header = 0x30 or ((qos and 0x03) shl 1)
        if (retain) header = header or 0x01
        writePacket(header, variable.toByteArray())
    }

    private fun subscribe(input: BufferedInputStream, topic: String) {
        val topicBytes = topic.toByteArray(StandardCharsets.UTF_8)
        val packetId = nextPacketId()
        val payload = ByteArray(2 + 2 + topicBytes.size + 1)
        var i = 0
        payload[i++] = ((packetId shr 8) and 0xff).toByte()
        payload[i++] = (packetId and 0xff).toByte()
        payload[i++] = ((topicBytes.size shr 8) and 0xff).toByte()
        payload[i++] = (topicBytes.size and 0xff).toByte()
        topicBytes.copyInto(payload, i)
        i += topicBytes.size
        payload[i] = 1
        writePacket(0x82, payload)
        val ack = readPacket(input)
        check(ack.type == 9) { "MQTT SUBACK missing" }
    }

    private fun writeConnect(cfg: Config) {
        val variable = ArrayList<Byte>()
        appendUtf8(variable, "MQTT")
        variable += 4
        var flags = 0x02
        if (cfg.username.isNotEmpty()) flags = flags or 0x80
        if (cfg.password.isNotEmpty()) flags = flags or 0x40
        variable += flags.toByte()
        variable += 0
        variable += 60
        appendUtf8(variable, cfg.clientId.take(64))
        if (cfg.username.isNotEmpty()) appendUtf8(variable, cfg.username)
        if (cfg.password.isNotEmpty()) appendUtf8(variable, cfg.password)
        writePacket(0x10, variable.toByteArray())
    }

    @Synchronized
    private fun writePubAck(packetId: Int) {
        writeRaw(byteArrayOf(0x40, 0x02, ((packetId shr 8) and 0xff).toByte(), (packetId and 0xff).toByte()))
    }

    @Synchronized
    private fun writePacket(header: Int, payload: ByteArray) {
        val remaining = encodeRemainingLength(payload.size)
        val packet = ByteArray(1 + remaining.size + payload.size)
        packet[0] = header.toByte()
        remaining.copyInto(packet, 1)
        payload.copyInto(packet, 1 + remaining.size)
        writeRaw(packet)
    }

    @Synchronized
    private fun writeRaw(bytes: ByteArray) {
        val stream = output ?: throw EOFException("MQTT socket closed")
        stream.write(bytes)
        stream.flush()
    }

    private fun readPacket(input: BufferedInputStream): Packet {
        val first = input.read()
        if (first < 0) throw EOFException("MQTT EOF")
        var multiplier = 1
        var remaining = 0
        var encoded: Int
        var count = 0
        do {
            encoded = input.read()
            if (encoded < 0) throw EOFException("MQTT EOF")
            remaining += (encoded and 127) * multiplier
            multiplier *= 128
            count++
            check(count <= 4) { "Malformed MQTT remaining length" }
        } while ((encoded and 128) != 0)
        val payload = ByteArray(remaining)
        var offset = 0
        while (offset < remaining) {
            val read = input.read(payload, offset, remaining - offset)
            if (read < 0) throw EOFException("MQTT EOF")
            offset += read
        }
        return Packet(type = (first shr 4) and 0x0f, flags = first and 0x0f, payload = payload)
    }

    private fun nextPacketId(): Int = packetIds.updateAndGet { if (it >= 65535) 1 else it + 1 }
    private fun topic(cfg: Config, leaf: String): String = "homeguard/v1/devices/${cfg.deviceId}/$leaf"

    private fun normalizeUri(raw: String): URI {
        val trimmed = raw.trim()
        return URI(if ("://" in trimmed) trimmed else "mqtts://$trimmed")
    }

    private fun appendUtf8(out: MutableList<Byte>, value: String) {
        val bytes = value.toByteArray(StandardCharsets.UTF_8)
        require(bytes.size <= 65535) { "MQTT string too long" }
        out += ((bytes.size shr 8) and 0xff).toByte()
        out += (bytes.size and 0xff).toByte()
        bytes.forEach(out::add)
    }

    private fun encodeRemainingLength(value: Int): ByteArray {
        var x = value
        val result = ArrayList<Byte>(4)
        do {
            var digit = x % 128
            x /= 128
            if (x > 0) digit = digit or 0x80
            result += digit.toByte()
        } while (x > 0)
        return result.toByteArray()
    }

    private data class Packet(val type: Int, val flags: Int, val payload: ByteArray)
}
