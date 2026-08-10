package ua.homeguard.s3.security

import org.junit.Assert.assertEquals
import org.junit.Test

class CloudAuthProofTest {
    @Test
    fun derivesFirmwareCompatiblePinDigestAndProof() {
        val digest = CloudAuthProof.derivePinDigest(
            userId = "admin",
            pin = "1234",
            saltHex = "00112233445566778899aabbccddeeff",
        )
        assertEquals(
            "9ad672bc1435ed9ac10bc8e9425beca9a200daf5c3e4aaec639e0626c0e9be52",
            digest,
        )

        val proof = CloudAuthProof.proof(
            requestId = "req-1",
            command = "security.arm_home",
            nonceHex = "00112233445566778899aabbccddeeff",
            pinDigestHex = digest,
        )
        assertEquals(
            "023f81ed3781bc47b6dd4ad47ece4e97613065c427be92d2b13cf074afe6b35b",
            proof,
        )
    }

    @Test
    fun buildsFirmwareCompatibleAdminPayloadAndEncryptedPin() {
        val digest = "9ad672bc1435ed9ac10bc8e9425beca9a200daf5c3e4aaec639e0626c0e9be52"
        val requestId = "req-admin-1"
        val command = "access.users.upsert"
        val encryptedPin = CloudAuthProof.encryptAdminPin(
            pin = "2468",
            requestId = requestId,
            command = command,
            pinDigestHex = digest,
        )
        assertEquals("f34c1085", encryptedPin)

        val canonical = CloudAuthProof.canonicalUpsert(
            targetId = "user1",
            name = "User One",
            role = "user",
            enabled = true,
            encryptedPinHex = encryptedPin,
        )
        assertEquals("5:user1|8:User One|4:user|4:true|8:f34c1085|", canonical)
        assertEquals(
            "21731e44d569974de462f40d1ad85a679410ee385d7bb3f0b3182599074dde0b",
            CloudAuthProof.adminPayloadProof(requestId, command, digest, canonical),
        )
        assertEquals("5:user1|", CloudAuthProof.canonicalTarget("user1"))
    }
}
