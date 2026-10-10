package ua.homeguard.s3.network.mqtt

import org.json.JSONObject

/** Immutable server-signed packet. Android neither holds the signing key nor transports a PIN. */
class SignedCommandEnvelope private constructor(
    val deviceId: String,
    val requestId: String,
    val issuedAtMs: Long,
    val expiresAtMs: Long,
    private val body: String,
) {
    fun payloadFor(deviceId: String, nowMs: Long = System.currentTimeMillis()): String {
        require(this.deviceId == deviceId) { "MQTT command device mismatch" }
        require(nowMs <= expiresAtMs && issuedAtMs <= nowMs + 30_000L) { "MQTT command expired or future dated" }
        return body
    }

    companion object {
        private val fields = setOf("version", "deviceId", "requestId", "actor", "command", "keyEpoch",
            "counter", "issuedAtMs", "expiresAtMs", "challenge", "signature")

        fun parse(json: JSONObject): SignedCommandEnvelope {
            val keys = json.keys().asSequence().toSet()
            require(keys == fields) { "Invalid signed command fields" }
            fun integer(name: String): Long {
                val raw = json.opt(name)
                require(raw is Number) { "Invalid $name" }
                return requireNotNull(raw.toString().toLongOrNull()) { "Invalid $name" }
            }
            fun text(name: String, allowEmpty: Boolean = false): String {
                val value = json.opt(name)
                require(value is String && (allowEmpty || value.isNotEmpty())) { "Invalid $name" }
                require(value.none { it.code < 32 || it.code == 127 }) { "Invalid canonical text" }
                return value
            }
            require(integer("version") == 1L) { "Unsupported command version" }
            val device = text("deviceId")
            val request = text("requestId")
            require(request.toByteArray(Charsets.UTF_8).size <= 128) { "Request id too long" }
            text("actor")
            val command = text("command")
            require(integer("keyEpoch") > 0 && integer("counter") > 0) { "Invalid command epoch/counter" }
            val issued = integer("issuedAtMs")
            val expires = integer("expiresAtMs")
            require(issued >= 0 && expires > issued && expires - issued <= 120_000L) { "Invalid command lifetime" }
            val challenge = text("challenge", allowEmpty = true)
            if (command == "security.disarm") require(challenge.matches(Regex("[0-9a-fA-F]{32}"))) { "Disarm challenge required" }
            val signature = text("signature")
            require(signature.length % 2 == 0 && signature.matches(Regex("[0-9a-fA-F]+"))) { "Invalid signature encoding" }
            // Preserve the signed fields exactly; the device verifies their signature and replay admission.
            return SignedCommandEnvelope(device, request, issued, expires, json.toString())
        }
    }
}
