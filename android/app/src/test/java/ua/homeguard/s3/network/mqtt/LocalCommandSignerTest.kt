package ua.homeguard.s3.network.mqtt

import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test
import java.security.KeyPairGenerator
import java.security.Signature
import java.security.spec.ECGenParameterSpec

class LocalCommandSignerTest {
    @Test fun persistenceFailureStopsBeforePrivateKeyAccess() {
        var keyReads = 0
        val signer = LocalCommandSigner("HG-TEST", "admin", 1,
            { keyReads++; error("Key must not be used") }, { error("Counter write failed") })
        try { signer.sign("HG-TEST", "output.lock"); fail("Unsigned counter admitted") }
        catch (error: IllegalStateException) { assertEquals("Counter write failed", error.message) }
        assertEquals(0, keyReads)
    }

    @Test fun aSigningFailureConsumesItsCounterSoTheNextAttemptCannotReuseIt() {
        val key = KeyPairGenerator.getInstance("EC").apply { initialize(ECGenParameterSpec("secp256r1")) }.generateKeyPair()
        var counter = 40L
        var keyAvailable = false
        val signer = LocalCommandSigner("HG-TEST", "admin", 1,
            { check(keyAvailable) { "Key unavailable" }; key.private }, { ++counter }, { 1_800_000_000_000 })
        try { signer.sign("HG-TEST", "output.lock"); fail("Unavailable key admitted") }
        catch (_: IllegalStateException) { }
        assertEquals(41L, counter)
        keyAvailable = true
        val packet = JSONObject(signer.sign("HG-TEST", "output.lock").payloadFor("HG-TEST", 1_800_000_000_000))
        assertEquals(42L, packet.getLong("counter"))
    }

    @Test fun phoneSignatureMatchesFirmwareTranscriptAndBindsDeviceActorAndCounter() {
        val key = KeyPairGenerator.getInstance("EC").apply { initialize(ECGenParameterSpec("secp256r1")) }.generateKeyPair()
        var counter = 40L
        val signer = LocalCommandSigner("HG-TEST", "admin", 7, { key.private }, { ++counter }, { 1_800_000_000_000 })
        val json = JSONObject(signer.sign("HG-TEST", "output.lock").payloadFor("HG-TEST", 1_800_000_000_000))
        assertEquals(41L, json.getLong("counter"))
        val fields = listOf("version", "deviceId", "requestId", "actor", "command", "keyEpoch", "counter", "issuedAtMs", "expiresAtMs", "challenge")
        val signature = json.getString("signature").chunked(2).map { it.toInt(16).toByte() }.toByteArray()
        fun verifies(): Boolean = Signature.getInstance("SHA256withECDSA").run {
            initVerify(key.public)
            update(fields.joinToString("\n") { "$it=${json.get(it)}" }.toByteArray(Charsets.UTF_8))
            verify(signature)
        }
        assertTrue(verifies())
        json.put("actor", "other-user")
        assertFalse(verifies())
        assertEquals(42L, JSONObject(signer.sign("HG-TEST", "security.arm_home").payloadFor("HG-TEST", 1_800_000_000_000)).getLong("counter"))
    }

    @Test fun invalidDeviceAndChallengeNeverAllocateCounterOrAccessKey() {
        val signer = LocalCommandSigner("HG-TEST", "admin", 1, { error("Key must not be accessed") },
            { error("Counter must not be allocated") })
        for ((device, command, challenge) in listOf(Triple("HG-OTHER", "output.lock", ""),
            Triple("HG-TEST", "security.disarm", ""), Triple("HG-TEST", "output.lock", "unexpected"))) {
            try { signer.sign(device, command, challenge); fail("Invalid request accepted") }
            catch (_: IllegalArgumentException) { }
        }
    }
}
