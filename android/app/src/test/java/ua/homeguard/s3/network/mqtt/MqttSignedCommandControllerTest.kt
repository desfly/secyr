package ua.homeguard.s3.network.mqtt

import kotlinx.coroutines.runBlocking
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test

class MqttSignedCommandControllerTest {
    private fun envelope(command: String, challenge: String): SignedCommandEnvelope {
        val now = System.currentTimeMillis()
        return SignedCommandEnvelope.parse(JSONObject().put("version", 1).put("deviceId", "HG-TEST")
            .put("requestId", command).put("actor", "session").put("command", command).put("keyEpoch", 1)
            .put("counter", 1).put("issuedAtMs", now).put("expiresAtMs", now + 60_000)
            .put("challenge", challenge).put("signature", "abcd"))
    }

    @Test fun disarmWaitsForCorrelatedChallengeBeforeSigningAction() = runBlocking {
        val actions = mutableListOf<String>()
        val token = "0123456789abcdef0123456789abcdef"
        val controller = MqttSignedCommandController("HG-TEST", { device, command, challenge ->
            assertEquals("HG-TEST", device)
            actions += "sign:$command"
            if (command == "security.disarm") assertEquals(token, challenge)
            envelope(command, challenge)
        }, { packet ->
            actions += "publish:${packet.requestId}"
            if (packet.requestId == "security.disarm_challenge")
                JSONObject().put("ok", true).put("requestId", packet.requestId).put("challenge", token).put("ttlMs", 60_000)
            else JSONObject().put("ok", true).put("requestId", packet.requestId)
        })
        assertTrue(controller.execute("security.disarm").getBoolean("ok"))
        assertEquals(listOf("sign:security.disarm_challenge", "publish:security.disarm_challenge",
            "sign:security.disarm", "publish:security.disarm"), actions)
    }

    @Test fun rejectedChallengeStopsBeforeSigningDisarm() = runBlocking {
        var signed = 0
        val controller = MqttSignedCommandController("HG-TEST", { _, command, challenge ->
            signed++; envelope(command, challenge)
        }, { JSONObject().put("ok", false).put("reason", "not_authorized") })
        assertFalse(controller.execute("security.disarm").getBoolean("ok"))
        assertEquals(1, signed)
    }

    @Test fun malformedChallengeAndUnknownOutcomeNeverTriggerSecondCommand() = runBlocking {
        for (unknown in listOf(false, true)) {
            var signed = 0
            var sent = 0
            val controller = MqttSignedCommandController("HG-TEST", { _, command, challenge ->
                signed++; envelope(command, challenge)
            }, { packet ->
                sent++
                if (unknown) throw MqttCommandOutcomeUnknown(packet.requestId)
                JSONObject().put("ok", true).put("challenge", "invalid").put("ttlMs", 60_000)
            })
            try { controller.execute("security.disarm"); fail("Invalid challenge accepted") }
            catch (_: java.io.IOException) { }
            assertEquals(1, signed)
            assertEquals(1, sent)
        }
    }
}
