package ua.homeguard.s3.network.mqtt

import org.json.JSONObject
import java.security.PrivateKey
import java.security.Signature
import java.util.UUID

/** Signs on the phone with an enrolled key. Counter allocation must commit durably before returning. */
class LocalCommandSigner(
    private val deviceId: String,
    private val actor: String,
    private val keyEpoch: Long,
    private val privateKey: () -> PrivateKey,
    private val nextCounter: () -> Long,
    private val clock: () -> Long = System::currentTimeMillis,
) {
    init {
        require(deviceId.matches(Regex("[A-Za-z0-9_-]+")))
        require(actor.isNotBlank() && actor.none { it.code < 32 || it.code == 127 })
        require(keyEpoch > 0)
    }

    @Synchronized
    fun sign(selectedDeviceId: String, command: String, challenge: String = ""): SignedCommandEnvelope {
        require(selectedDeviceId == deviceId) { "Signing device mismatch" }
        require(command in setOf("security.arm_home", "security.arm_away", "security.disarm",
            "security.disarm_challenge", "security.panic", "output.lock"))
        require(if (command == "security.disarm") challenge.matches(Regex("[0-9a-fA-F]{32}")) else challenge.isEmpty())
        val now = clock()
        require(now >= 0 && now <= Long.MAX_VALUE - 60_000)
        val counter = nextCounter()
        require(counter > 0) { "Invalid durable command counter" }
        val fields = linkedMapOf<String, Any>(
            "version" to 1, "deviceId" to deviceId, "requestId" to UUID.randomUUID().toString(),
            "actor" to actor, "command" to command, "keyEpoch" to keyEpoch, "counter" to counter,
            "issuedAtMs" to now, "expiresAtMs" to now + 60_000, "challenge" to challenge,
        )
        val transcript = fields.entries.joinToString("\n") { "${it.key}=${it.value}" }.toByteArray(Charsets.UTF_8)
        val signature = Signature.getInstance("SHA256withECDSA").run {
            initSign(privateKey())
            update(transcript)
            sign().joinToString("") { "%02x".format(it.toInt() and 255) }
        }
        val packet = JSONObject()
        fields.forEach { (name, value) -> packet.put(name, value) }
        packet.put("signature", signature)
        return SignedCommandEnvelope.parse(packet)
    }
}
