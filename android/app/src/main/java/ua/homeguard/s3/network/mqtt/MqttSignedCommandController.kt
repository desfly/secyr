package ua.homeguard.s3.network.mqtt

import kotlinx.coroutines.sync.Mutex
import kotlinx.coroutines.sync.withLock
import org.json.JSONObject
import java.io.IOException

/** One configured instance per device. UI routing and signer credentials are provisioned separately. */
class MqttSignedCommandController(
    private val deviceId: String,
    private val sign: suspend (String, String, String) -> SignedCommandEnvelope,
    private val publish: suspend (SignedCommandEnvelope) -> JSONObject,
) {
    constructor(deviceId: String, signer: SignedCommandApi, mqtt: MqttRuntimeClient) :
        this(deviceId, { device, command, challenge -> signer.request(device, command, challenge) },
            { envelope -> mqtt.publishSignedCommand(envelope) })

    private val gate = Mutex()

    suspend fun execute(command: String): JSONObject = gate.withLock {
        require(command in setOf("security.arm_home", "security.arm_away", "security.disarm",
            "security.panic", "output.lock")) { "Unsupported MQTT action" }
        var challenge = ""
        if (command == "security.disarm") {
            val response = send("security.disarm_challenge", "")
            if (response.opt("ok") == false) return@withLock response
            challenge = response.opt("challenge") as? String ?: throw IOException("Missing disarm challenge")
            val ttl = (response.opt("ttlMs") as? Number)?.toString()?.toLongOrNull()
            if (response.opt("ok") != true || !challenge.matches(Regex("[0-9a-f]{32}")) ||
                ttl == null || ttl !in 1L..60_000L) throw IOException("Invalid disarm challenge response")
        }
        send(command, challenge)
    }

    private suspend fun send(command: String, challenge: String): JSONObject {
        val envelope = sign(deviceId, command, challenge)
        envelope.payloadFor(deviceId)
        return publish(envelope)
    }
}
