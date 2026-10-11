package ua.homeguard.s3.network.mqtt

import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test
import java.security.KeyFactory
import java.security.Signature
import java.security.spec.X509EncodedKeySpec

/** One Python-generated vector shared with the backend suite; no private key is retained. */
class SignerInteropTest {
    private fun vector(): JSONObject = JSONObject(
        requireNotNull(javaClass.getResourceAsStream("/signed-command-vector.json"))
            .bufferedReader().use { it.readText() },
    )

    private fun hex(value: String): ByteArray = value.chunked(2).map { it.toInt(16).toByte() }.toByteArray()

    private fun verifies(vector: JSONObject, packet: JSONObject): Boolean {
        val fields = listOf("version", "deviceId", "requestId", "actor", "command", "keyEpoch",
            "counter", "issuedAtMs", "expiresAtMs", "challenge")
        val transcript = fields.joinToString("\n") { "$it=${packet.get(it)}" }.toByteArray(Charsets.UTF_8)
        val key = KeyFactory.getInstance("EC").generatePublic(X509EncodedKeySpec(hex(vector.getString("publicKeyDerHex"))))
        return Signature.getInstance("SHA256withECDSA").run {
            initVerify(key)
            update(transcript)
            verify(hex(packet.getString("signature")))
        }
    }

    @Test fun pythonPacketRemainsValidAfterAndroidParseAndPublicationEncoding() {
        val vector = vector()
        val original = vector.getJSONObject("envelope")
        val envelope = SignedCommandEnvelope.parse(original)
        val outgoing = JSONObject(envelope.payloadFor("HG-INTEROP", original.getLong("issuedAtMs")))
        assertTrue(verifies(vector, outgoing))
        for (field in original.keys()) assertEquals(original.get(field), outgoing.get(field))
        assertEquals(42L, outgoing.getLong("counter"))
        assertEquals("security.disarm", outgoing.getString("command"))
    }

    @Test fun changingAnAuthorizedFieldInvalidatesPythonSignature() {
        val vector = vector()
        val original = vector.getJSONObject("envelope")
        for ((field, value) in listOf("actor" to "other-session", "deviceId" to "HG-OTHER",
            "command" to "security.arm_away", "counter" to 43L,
            "challenge" to "fedcba9876543210fedcba9876543210")) {
            val changed = JSONObject(original.toString()).put(field, value)
            assertFalse("Tampered $field", verifies(vector, changed))
        }
    }
}
