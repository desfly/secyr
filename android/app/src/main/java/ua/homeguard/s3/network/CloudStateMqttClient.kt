package ua.homeguard.s3.network

import com.hivemq.client.mqtt.MqttClient
import com.hivemq.client.mqtt.datatypes.MqttQos
import com.hivemq.client.mqtt.mqtt3.Mqtt3AsyncClient
import java.nio.charset.StandardCharsets
import java.util.UUID
import java.util.concurrent.ConcurrentHashMap
import kotlinx.coroutines.CompletableDeferred
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.withTimeout
import org.json.JSONObject
import ua.homeguard.s3.model.CommandReply
import ua.homeguard.s3.security.CloudAuthProof

/**
 * Build-0060 cloud telemetry + command transport.
 *
 * The public Mosquitto endpoint is TEST-ONLY. Production must replace this
 * with an authenticated HomeGuard broker/service endpoint and per-device ACLs.
 */
class CloudStateMqttClient(
    private val telemetry: TelemetrySocket,
) {
    companion object {
        private const val TEST_BROKER_HOST = "test.mosquitto.org"
        private const val TEST_BROKER_PORT = 8886
        private const val RESPONSE_TIMEOUT_MS = 10_000L
    }

    private var client: Mqtt3AsyncClient? = null
    private var activeDeviceId: String = ""
    @Volatile private var connected = false
    private val waiters = ConcurrentHashMap<String, CompletableDeferred<JSONObject>>()
    private val mutableStatus = MutableStateFlow("cloud idle")
    val status: StateFlow<String> = mutableStatus

    @Synchronized
    fun connect(deviceId: String) {
        val normalized = deviceId.trim()
        if (normalized.isBlank()) {
            disconnect()
            return
        }
        if (activeDeviceId == normalized && client != null) return

        disconnect()
        activeDeviceId = normalized
        mutableStatus.value = "cloud connecting"

        val mqtt = MqttClient.builder()
            .identifier("hg-android-${UUID.randomUUID()}")
            .serverHost(TEST_BROKER_HOST)
            .serverPort(TEST_BROKER_PORT)
            .sslWithDefaultConfig()
            .useMqttVersion3()
            .buildAsync()
        client = mqtt

        mqtt.connect().whenComplete { _, connectError ->
            if (client !== mqtt) return@whenComplete
            if (connectError != null) {
                connected = false
                mutableStatus.value = "cloud error: ${connectError.message ?: "connect"}"
                return@whenComplete
            }

            connected = true
            val stateTopic = "homeguard/v1/devices/$normalized/state"
            val responseTopic = "homeguard/v1/devices/$normalized/responses"

            mqtt.subscribeWith()
                .topicFilter(stateTopic)
                .qos(MqttQos.AT_LEAST_ONCE)
                .callback { publish ->
                    if (client !== mqtt) return@callback
                    val payload = String(publish.payloadAsBytes, StandardCharsets.UTF_8)
                    if (telemetry.acceptCloudState(payload)) mutableStatus.value = "cloud connected"
                }
                .send()
                .whenComplete { _, subscribeError ->
                    if (client !== mqtt) return@whenComplete
                    if (subscribeError != null) mutableStatus.value = "cloud error: ${subscribeError.message ?: "state subscribe"}"
                }

            mqtt.subscribeWith()
                .topicFilter(responseTopic)
                .qos(MqttQos.AT_LEAST_ONCE)
                .callback { publish ->
                    if (client !== mqtt) return@callback
                    runCatching {
                        val json = JSONObject(String(publish.payloadAsBytes, StandardCharsets.UTF_8))
                        val requestId = json.optString("request_id")
                        if (requestId.isNotBlank()) waiters.remove(requestId)?.complete(json)
                    }
                }
                .send()
                .whenComplete { _, subscribeError ->
                    if (client !== mqtt) return@whenComplete
                    mutableStatus.value = if (subscribeError == null) "cloud subscribed" else "cloud error: ${subscribeError.message ?: "response subscribe"}"
                }
        }
    }

    suspend fun executeCommand(command: String, actor: String, pin: String): CommandReply {
        val mqtt = client ?: return CommandReply(false, code = "cloud_offline")
        val deviceId = activeDeviceId
        if (!connected || deviceId.isBlank()) return CommandReply(false, code = "cloud_offline")
        if (actor.isBlank() || pin.length !in 4..12) return CommandReply(false, code = "credentials_required")

        val requestId = "android-${UUID.randomUUID()}"
        return runCatching {
            val challengeWaiter = CompletableDeferred<JSONObject>()
            waiters[requestId] = challengeWaiter
            publish(mqtt, deviceId, JSONObject()
                .put("request_id", requestId)
                .put("command", "auth.challenge")
                .put("actor", actor)
                .put("target_command", command))

            val challenge = withTimeout(RESPONSE_TIMEOUT_MS) { challengeWaiter.await() }
            if (!challenge.optBoolean("ok") || challenge.optString("code") != "auth_challenge") {
                return@runCatching CommandReply(false, code = challenge.optString("code", "challenge_failed"))
            }

            val nonce = challenge.optString("nonce")
            val salt = challenge.optString("salt")
            val digest = CloudAuthProof.derivePinDigest(actor, pin, salt)
            val proof = CloudAuthProof.proof(requestId, command, nonce, digest)

            val resultWaiter = CompletableDeferred<JSONObject>()
            waiters[requestId] = resultWaiter
            publish(mqtt, deviceId, JSONObject()
                .put("request_id", requestId)
                .put("command", command)
                .put("actor", actor)
                .put("auth_proof", proof))

            val result = withTimeout(RESPONSE_TIMEOUT_MS) { resultWaiter.await() }
            CommandReply(
                accepted = result.optBoolean("ok"),
                code = result.optString("code", "cloud_reply"),
            )
        }.getOrElse { error ->
            waiters.remove(requestId)?.cancel()
            CommandReply(false, code = "cloud_${error::class.java.simpleName.lowercase()}")
        }
    }

    private fun publish(mqtt: Mqtt3AsyncClient, deviceId: String, json: JSONObject) {
        mqtt.publishWith()
            .topic("homeguard/v1/devices/$deviceId/commands")
            .qos(MqttQos.AT_LEAST_ONCE)
            .payload(json.toString().toByteArray(StandardCharsets.UTF_8))
            .send()
    }

    @Synchronized
    fun disconnect() {
        val previous = client
        client = null
        connected = false
        activeDeviceId = ""
        waiters.values.forEach { it.cancel() }
        waiters.clear()
        if (previous != null) runCatching { previous.disconnect() }
        mutableStatus.value = "cloud idle"
    }
}
