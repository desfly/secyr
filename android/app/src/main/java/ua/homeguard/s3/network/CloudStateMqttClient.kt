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
import ua.homeguard.s3.security.CloudAdminUser
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

    private data class AuthContext(
        val mqtt: Mqtt3AsyncClient,
        val deviceId: String,
        val requestId: String,
        val actor: String,
        val digest: String,
        val proof: String,
    )

    private var client: Mqtt3AsyncClient? = null
    private var activeDeviceId: String = ""
    @Volatile private var connected = false
    @Volatile private var stateSubscribed = false
    private val waiters = ConcurrentHashMap<String, CompletableDeferred<JSONObject>>()
    private val mutableStatus = MutableStateFlow("cloud idle")
    private val mutableProfile = MutableStateFlow(CloudSessionProfile.locked())
    private val mutableAdminUsers = MutableStateFlow<List<CloudAdminUser>>(emptyList())
    private val mutableAdminStatus = MutableStateFlow("Admin: не завантажено")
    val status: StateFlow<String> = mutableStatus
    val profile: StateFlow<CloudSessionProfile> = mutableProfile
    val adminUsers: StateFlow<List<CloudAdminUser>> = mutableAdminUsers
    val adminStatus: StateFlow<String> = mutableAdminStatus

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

    private suspend fun awaitResponse(requestId: String, waiter: CompletableDeferred<JSONObject>): JSONObject =
        try {
            withTimeout(RESPONSE_TIMEOUT_MS) { waiter.await() }
        } finally {
            waiters.remove(requestId, waiter)
        }

    private suspend fun beginAuthenticated(command: String, actor: String, pin: String): AuthContext {
        val mqtt = client ?: error("cloud_offline")
        val deviceId = activeDeviceId
        check(connected && deviceId.isNotBlank()) { "cloud_offline" }
        require(actor.isNotBlank() && pin.length in 4..12) { "credentials_required" }

        val requestId = "android-${UUID.randomUUID()}"
        val challengeWaiter = CompletableDeferred<JSONObject>()
        waiters[requestId] = challengeWaiter
        publish(
            mqtt,
            deviceId,
            JSONObject()
                .put("request_id", requestId)
                .put("command", "auth.challenge")
                .put("actor", actor)
                .put("target_command", command),
        )
        val challenge = awaitResponse(requestId, challengeWaiter)
        check(challenge.optBoolean("ok") && challenge.optString("code") == "auth_challenge") {
            challenge.optString("code", "challenge_failed")
        }
        val digest = CloudAuthProof.derivePinDigest(actor, pin, challenge.optString("salt"))
        val proof = CloudAuthProof.proof(requestId, command, challenge.optString("nonce"), digest)
        return AuthContext(mqtt, deviceId, requestId, actor, digest, proof)
    }

    private suspend fun finishAuthenticated(context: AuthContext, command: String, extra: JSONObject? = null): JSONObject {
        val resultWaiter = CompletableDeferred<JSONObject>()
        waiters[context.requestId] = resultWaiter
        val payload = JSONObject()
            .put("request_id", context.requestId)
            .put("command", command)
            .put("actor", context.actor)
            .put("auth_proof", context.proof)
        if (extra != null) {
            val keys = extra.keys()
            while (keys.hasNext()) {
                val key = keys.next()
                payload.put(key, extra.get(key))
            }
        }
        publish(context.mqtt, context.deviceId, payload)
        return awaitResponse(context.requestId, resultWaiter)
    }

    private suspend fun authenticatedJson(command: String, actor: String, pin: String): JSONObject {
        val context = beginAuthenticated(command, actor, pin)
        return finishAuthenticated(context, command)
    }

    suspend fun loginProfile(actor: String, pin: String): CloudSessionProfile {
        val normalizedActor = actor.trim()
        val result = runCatching { authenticatedJson("profile.self", normalizedActor, pin) }.getOrElse {
            logoutProfile()
            mutableStatus.value = "cloud login failed"
            return mutableProfile.value
        }
        if (!result.optBoolean("ok") || result.optString("code") != "self_profile") {
            logoutProfile()
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
        mutableAdminUsers.value = emptyList()
        mutableAdminStatus.value = "Admin: не завантажено"
        mutableStatus.value = if (profile.sensorOnly) "cloud connected · guest sensor-only" else "cloud authenticated · ${profile.role.name.lowercase()}"
        subscribeFullStateIfAllowed(profile)
        if (profile.sensorOnly) refreshGuestSensors(normalizedActor, pin)
        if (profile.canManageUsers) refreshAdminUsers(normalizedActor, pin)
        return profile
    }

    fun logoutProfile() {
        mutableProfile.value = CloudSessionProfile.locked()
        mutableAdminUsers.value = emptyList()
        mutableAdminStatus.value = "Admin: не авторизовано"
        mutableStatus.value = if (connected) "cloud locked · login required" else "cloud idle"
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

    suspend fun refreshAdminUsers(actor: String, pin: String): Boolean {
        val profile = mutableProfile.value
        if (!profile.canManageUsers || profile.id != actor.trim()) return false
        mutableAdminStatus.value = "Admin: завантаження…"
        return runCatching {
            val result = authenticatedJson("access.users.list", actor.trim(), pin)
            if (!result.optBoolean("ok") || result.optString("code") != "users") error(result.optString("code", "users_list"))
            mutableAdminUsers.value = parseAdminUsers(result)
            mutableAdminStatus.value = "Admin: ${mutableAdminUsers.value.size}/8 користувачів"
            true
        }.getOrElse { error ->
            mutableAdminStatus.value = "Admin помилка: ${error.message ?: "users"}"
            false
        }
    }

    suspend fun upsertAdminUser(
        actor: String,
        pin: String,
        targetId: String,
        name: String,
        role: String,
        enabled: Boolean,
        newPin: String,
    ): Boolean {
        val profile = mutableProfile.value
        if (!profile.canManageUsers || profile.id != actor.trim()) return false
        require(targetId.isNotBlank() && targetId.length <= 23)
        require(name.length <= 31)
        require(role in setOf("admin", "user", "guest"))
        require(newPin.length in 4..12)
        mutableAdminStatus.value = "Admin: збереження $targetId…"
        return runCatching {
            val command = "access.users.upsert"
            val context = beginAuthenticated(command, actor.trim(), pin)
            val encryptedPin = CloudAuthProof.encryptAdminPin(newPin, context.requestId, command, context.digest)
            val canonical = CloudAuthProof.canonicalUpsert(targetId, name, role, enabled, encryptedPin)
            val payloadProof = CloudAuthProof.adminPayloadProof(context.requestId, command, context.digest, canonical)
            val result = finishAuthenticated(
                context,
                command,
                JSONObject()
                    .put("target_id", targetId)
                    .put("name", name)
                    .put("role", role)
                    .put("enabled", enabled.toString())
                    .put("pin_enc", encryptedPin)
                    .put("payload_proof", payloadProof),
            )
            if (!result.optBoolean("ok") || result.optString("code") != "users") error(result.optString("code", "users_upsert"))
            mutableAdminUsers.value = parseAdminUsers(result)
            mutableAdminStatus.value = "Admin: користувача $targetId збережено"
            true
        }.getOrElse { error ->
            mutableAdminStatus.value = "Admin відхилено: ${error.message ?: "upsert"}"
            false
        }
    }

    suspend fun adminUserAction(actor: String, pin: String, command: String, targetId: String): Boolean {
        val profile = mutableProfile.value
        if (!profile.canManageUsers || profile.id != actor.trim()) return false
        require(command in setOf("access.users.enable", "access.users.disable", "access.users.delete"))
        require(targetId.isNotBlank() && targetId.length <= 23)
        mutableAdminStatus.value = "Admin: $command $targetId…"
        return runCatching {
            val context = beginAuthenticated(command, actor.trim(), pin)
            val canonical = CloudAuthProof.canonicalTarget(targetId)
            val payloadProof = CloudAuthProof.adminPayloadProof(context.requestId, command, context.digest, canonical)
            val result = finishAuthenticated(
                context,
                command,
                JSONObject()
                    .put("target_id", targetId)
                    .put("payload_proof", payloadProof),
            )
            if (!result.optBoolean("ok") || result.optString("code") != "users") error(result.optString("code", "users_action"))
            mutableAdminUsers.value = parseAdminUsers(result)
            mutableAdminStatus.value = "Admin: OK $targetId"
            true
        }.getOrElse { error ->
            mutableAdminStatus.value = "Admin відхилено: ${error.message ?: "action"}"
            false
        }
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

    private fun parseAdminUsers(result: JSONObject): List<CloudAdminUser> {
        val array = result.optJSONArray("users") ?: return emptyList()
        return buildList {
            for (index in 0 until array.length()) {
                val item = array.optJSONObject(index) ?: continue
                add(
                    CloudAdminUser.fromController(
                        id = item.optString("id"),
                        name = item.optString("name"),
                        role = item.optString("role"),
                        enabled = item.optBoolean("enabled"),
                    ),
                )
            }
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
        stateSubscribed = false
        activeDeviceId = ""
        mutableProfile.value = CloudSessionProfile.locked()
        mutableAdminUsers.value = emptyList()
        mutableAdminStatus.value = "Admin: не підключено"
        waiters.values.forEach { it.cancel() }
        waiters.clear()
        if (previous != null) runCatching { previous.disconnect() }
        mutableStatus.value = "cloud idle"
    }
}
