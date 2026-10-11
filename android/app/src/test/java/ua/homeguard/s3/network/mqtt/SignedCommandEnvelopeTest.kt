package ua.homeguard.s3.network.mqtt

import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test

class SignedCommandEnvelopeTest {
    private fun packet(): JSONObject = JSONObject()
        .put("version", 1).put("deviceId", "HG-TEST").put("requestId", "request-1")
        .put("actor", "user-1").put("command", "security.arm_away")
        .put("keyEpoch", 2).put("counter", 42).put("issuedAtMs", 100_000L)
        .put("expiresAtMs", 120_000L).put("challenge", "").put("signature", "abcd")

    private fun rejected(block: () -> Unit) {
        try { block(); fail("Invalid packet accepted") } catch (_: IllegalArgumentException) { }
    }

    @Test fun preservesSignedFieldsAndDoesNotRetainMutableInput() {
        val json = packet()
        val envelope = SignedCommandEnvelope.parse(json)
        json.put("actor", "changed")
        val sent = JSONObject(envelope.payloadFor("HG-TEST", 110_000L))
        assertEquals("user-1", sent.getString("actor"))
        assertEquals(42L, sent.getLong("counter"))
        assertEquals("abcd", sent.getString("signature"))
        assertFalse(sent.has("credential"))
    }

    @Test fun rejectsWrongDeviceExpiredAndFutureCommands() {
        val envelope = SignedCommandEnvelope.parse(packet())
        rejected { envelope.payloadFor("HG-OTHER", 110_000) }
        rejected { envelope.payloadFor("HG-TEST", 120_001) }
        rejected { envelope.payloadFor("HG-TEST", 60_000) }
    }

    @Test fun rejectsPinUnsignedAndAmbiguousCanonicalFields() {
        rejected { SignedCommandEnvelope.parse(packet().put("credential", "1234")) }
        rejected { SignedCommandEnvelope.parse(packet().apply { remove("signature") }) }
        rejected { SignedCommandEnvelope.parse(packet().put("actor", "user\ncommand=security.disarm")) }
        rejected { SignedCommandEnvelope.parse(packet().put("signature", "xyz")) }
        rejected { SignedCommandEnvelope.parse(packet().put("counter", 1.5)) }
        rejected { SignedCommandEnvelope.parse(packet().put("keyEpoch", 0)) }
        rejected { SignedCommandEnvelope.parse(packet().put("challenge", JSONObject.NULL)) }
        rejected { SignedCommandEnvelope.parse(packet().put("expiresAtMs", 220_001)) }
    }

    @Test fun disarmRequiresFirmwareChallenge() {
        rejected { SignedCommandEnvelope.parse(packet().put("command", "security.disarm")) }
        val valid = packet().put("command", "security.disarm").put("challenge", "0123456789abcdef0123456789abcdef")
        assertEquals(valid.toString(), SignedCommandEnvelope.parse(valid).payloadFor("HG-TEST", 110_000))
    }
}
