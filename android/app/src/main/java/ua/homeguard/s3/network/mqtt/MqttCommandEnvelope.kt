package ua.homeguard.s3.network.mqtt

import org.json.JSONObject

data class MqttCommandEnvelope(
    val version: Long = 1,
    val deviceId: String,
    val requestId: String,
    val actor: String,
    val command: String,
    val keyEpoch: Long,
    val counter: Long,
    val issuedAtMs: Long,
    val expiresAtMs: Long,
    val challenge: String = "",
) {
    init {
        require(version == 1L) { "Unsupported MQTT command version" }
        require(deviceId.isNotBlank()) { "deviceId is required" }
        require(requestId.isNotBlank()) { "requestId is required" }
        require(actor.isNotBlank()) { "actor is required" }
        require(command.isNotBlank()) { "command is required" }
        require(keyEpoch > 0) { "keyEpoch must be positive" }
        require(counter > 0) { "counter must be positive" }
        require(expiresAtMs > issuedAtMs) { "invalid command freshness window" }
        require(expiresAtMs - issuedAtMs <= 120_000L) { "command TTL exceeds ESP protocol limit" }
        listOf(deviceId, requestId, actor, command, challenge).forEach {
            require(it.none { ch -> ch == '\n' || ch == '\r' || ch.code < 0x20 }) {
                "canonical field contains a control character"
            }
        }
    }

    fun canonical(): String =
        "version=$version\n" +
            "deviceId=$deviceId\n" +
            "requestId=$requestId\n" +
            "actor=$actor\n" +
            "command=$command\n" +
            "keyEpoch=$keyEpoch\n" +
            "counter=$counter\n" +
            "issuedAtMs=$issuedAtMs\n" +
            "expiresAtMs=$expiresAtMs\n" +
            "challenge=$challenge"

    fun signedJson(signer: MqttCommandSigner): JSONObject =
        JSONObject()
            .put("version", version)
            .put("deviceId", deviceId)
            .put("requestId", requestId)
            .put("actor", actor)
            .put("command", command)
            .put("keyEpoch", keyEpoch)
            .put("counter", counter)
            .put("issuedAtMs", issuedAtMs)
            .put("expiresAtMs", expiresAtMs)
            .put("challenge", challenge)
            .put("signature", signer.signCanonical(canonical()))
}
