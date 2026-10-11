package ua.homeguard.s3.control

import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.runBlocking
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test
import ua.homeguard.s3.model.CommandType
import ua.homeguard.s3.network.mqtt.MqttCommandOutcomeUnknown

class MqttCommandRouteTest {
    @Test fun dailyActionsMapToFirmwareCommandsAndReturnDeviceRejections() = runBlocking {
        val sent = mutableListOf<String>()
        val route = MqttCommandRoute { command ->
            sent += command
            JSONObject().put("ok", false).put("reason", "not_authorized")
        }
        for (action in listOf(CommandType.ARM_HOME, CommandType.ARM_AWAY, CommandType.DISARM)) {
            val reply = route.execute(action)!!
            assertFalse(reply.accepted)
            assertEquals("not_authorized", reply.code)
        }
        route.pulseLock()
        assertEquals(listOf("security.arm_home", "security.arm_away", "security.disarm", "output.lock"), sent)
    }

    @Test fun unconfiguredRouteAllowsExistingTransportsAndUnsupportedActionIsNotSent() = runBlocking {
        var sent = 0
        val route = MqttCommandRoute { sent++; null }
        assertNull(route.execute(CommandType.ARM_HOME))
        assertNull(route.execute(CommandType.OPEN_VALVES))
        assertEquals(1, sent)
    }

    @Test fun uncertainOutcomeIsNonNullAndDoesNotRetry() = runBlocking {
        var sent = 0
        val route = MqttCommandRoute { sent++; throw MqttCommandOutcomeUnknown("request-1") }
        val reply = route.pulseLock()!!
        assertFalse(reply.accepted)
        assertEquals("command_outcome_unknown", reply.code)
        assertEquals(1, sent)
    }

    @Test fun callerCancellationIsNotConvertedIntoFailure() = runBlocking {
        val route = MqttCommandRoute { throw CancellationException("cancelled") }
        try { route.execute(CommandType.DISARM); fail("Cancellation swallowed") }
        catch (_: CancellationException) { }
    }
}
