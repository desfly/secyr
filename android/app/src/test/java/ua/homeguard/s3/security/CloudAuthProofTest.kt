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
            userId = "admin",
            requestId = "req-1",
            command = "security.arm_home",
            nonceHex = "00112233445566778899aabbccddeeff",
            pinDigestHex = digest,
        )
        assertEquals(
            "5826e45b0a173e095f2601ff2de8b873ec7a52e59b73bbbff15b53d07ed5f700",
            proof,
        )
    }
}
