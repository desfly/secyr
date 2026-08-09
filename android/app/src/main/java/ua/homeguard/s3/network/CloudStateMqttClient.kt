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
import ua.homeguard.s3.security.CloudSessionProfile

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
    @Volatile private var stateSubscribed = false
    private val waiters = ConcurrentHashMap<String, CompletableDeferred<JSONObject>>()
    private val mutableStatus = MutableStateFlow("cloud idle")
    private val mutableProfile = MutableStateFlow(CloudSessionProfile.locked())
    val status: StateFlow<String> = mutableStatus
    val profile: StateFlow<CloudSessionProfile> = mutableProfile

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
            subscribeResponses(mqtt, normalized)
        }
    }

    private fun subscribeResponses(mqtt: Mqtt3AsyncClient, deviceId: String) {
        val responseTopic = "homeguard/v1/devices/$deviceId/responses"
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
            .whenComplete { _, error ->
                if (client !== mqtt) return@whenComplete
                mutableStatus.value = if (error == null) "cloud locked · login required" else "cloud error: ${error.message ?: "response subscribe"}"
            }
    }

    private fun subscribeFullStateIfAllowed(profile: CloudSessionProfile) {
        val mqtt = client ?: return
        val deviceId = activeDeviceId
        if (!connected || deviceId.isBlank() || !profile.canSubscribeFullState || stateSubscribed) return
        val stateTopic = "homeguard/v1/devices/$deviceId/state"
        mqtt.subscribeWith()
            .topicFilter(stateTopic)
            .qos(MqttQos.AT_LEAST_ONCE)
            .callback { publish ->
                if (client !== mqtt || !mutableProfile.value.canSubscribeFullState) return@callback
                val payload = String(publish.payloadAsBytes, StandardCharsets.UTF_8)
                if (telemetry.acceptCloudState(payload)) mutableStatus.value = "cloud connected · ${mutableProfile.value.role.name.lowercase()}"
            }
            .send()
            .whenComplete { _, error ->
                if (client !== mqtt) return@whenComplete
                stateSubscribed = error == null
                if (error != null) mutableStatus.value = "cloud error: ${error.message ?: "state subscribe"}"
            }
    }

    private suspend fun authenticatedJson(command: String, actor: String, pin: String): JSONObject {
        val mqtt = client ?: error("cloud_offline")
        val deviceId = activeDeviceId
        check(connected && deviceId.isNotBlank()) { "cloud_offline" }
        require(actor.isNotBlank() && pin.length in 4..12) { "credentials_required" }

        val requestId = "android-${UUID.randomUUID()}"
        val challengeWaiter = CompletableDeferred<JSONObject>()
        waiters[requestId] = challengeWaiter
        publish(mqtt, deviceId, JSONObject()
            .put("request_id", requestId)
            .put("command", "auth.challenge")
            .put("actor", actor)
            .put("target_command", command))

        val challenge = withTimeout(RESPONSE_TIMEOUT_MS) { challengeWaiter.await() }
        check(challenge.optBoolean("ok") && challenge.optString("code") == "auth_challenge") {
            challenge.optString("code", "challenge_failed")
        }

        val digest = CloudAuthProof.derivePinDigest(actor, pin, challenge.optString("salt"))
        val proof = CloudAuthProof.proof(requestId, command, challenge.optString("nonce"), digest)
        val resultWaiter = CompletableDeferred<JSONObject>()
        waiters[requestId] = resultWaiter
        publish(mqtt, deviceId, JSONObject()
            .put("request_id", requestId)
            .put("command", command)
            .put("actor", actor)
            .put("auth_proof", proof))
        return withTimeout(RESPONSE_TIMEOUT_MS) { resultWaiter.await() }
    }

    suspend fun loginProfile(actor: String, pin: String): CloudSessionProfile {
        val result = runCatching { authenticatedJson("profile.self", actor.trim(), pin) }.getOrElse {
            mutableProfile.value = CloudSessionProfile.locked()
            mutableStatus.value = "cloud login failed"
            return mutableProfile.value
        }
        if (!result.optBoolean("ok") || result.optString("code") != "self_profile") {
            mutableProfile.value = CloudSessionProfile.locked()
            mutableStatus.value = "cloud login rejected: ${result.optString("code", "profile")}"
            return mutableProfile.value
        }
        val profile = CloudSessionProfile.fromController(
            id = result.optString("id"),
            name = result.optString("name"),
            role = result.optString("role"),
            enabled = result.optBoolean("enabled"),
            canArm = result.optBoolean("can_arm"),
            canDisarm = result.optBoolean("can_disarm"),
            canControlValves = result.optBoolean("can_control_valves"),
            canManageUsers = result.optBoolean("can_manage_users"),
        )
        mutableProfile.value = profile
        mutableStatus.value = if (profile.sensorOnly) "cloud connected · guest sensor-only" else "cloud authenticated · ${profile.role.name.lowercase()}"
        subscribeFullStateIfAllowed(profile)
        if (profile.sensorOnly) refreshGuestSensors(actor, pin)
        return profile
    }

    suspend fun refreshGuestSensors(actor: String, pin: String): Boolean {
        val profile = mutableProfile.value
        if (!profile.sensorOnly || profile.id != actor.trim()) return false
        return runCatching {
            val result = authenticatedJson("sensors.status", actor.trim(), pin)
            val accepted = telemetry.acceptCloudSensorStatus(result.toString())
            if (accepted) mutableStatus.value = "cloud connected · guest sensor-only"
            accepted
        }.getOrDefault(false)
    }

    suspend fun executeCommand(command: String, actor: String, pin: String): CommandReply {
        val profile = mutableProfile.value
        if (!profile.authenticated || profile.id != actor.trim()) return CommandReply(false, code = "profile_login_required")
        val allowed = when (command) {
            "security.arm_home", "security.arm_away" -> profile.canArm
            "security.disarm" -> profile.canDisarm
            "valve.open", "valve.close" -> profile.canControlValves
            else -> false
        }
        if (!allowed) return CommandReply(false, code = "denied_role")
        return runCatching {
            val result = authenticatedJson(command, actor.trim(), pin)
            CommandReply(result.optBoolean("ok"), code = result.optString("code", "cloud_reply"))
        }.getOrElse { error -> CommandReply(false, code = "cloud_${error::class.java.simpleName.lowercase()}") }
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
        stateSubscribed = false
        activeDeviceId = ""
        mutableProfile.value = CloudSessionProfile.locked()
        waiters.values.forEach { it.cancel() }
        waiters.clear()
        if (previous != null) runCatching { previous.disconnect() }
        mutableStatus.value = "cloud idle"
    }
}
