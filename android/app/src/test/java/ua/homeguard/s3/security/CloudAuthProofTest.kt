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
}
