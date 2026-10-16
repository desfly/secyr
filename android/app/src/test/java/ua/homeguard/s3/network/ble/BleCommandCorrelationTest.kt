package ua.homeguard.s3.network.ble

import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test

class BleCommandCorrelationTest {
    @Test fun wrongMalformedAndDuplicateRepliesCannotAcknowledgeACommand() {
        val commands = BleCommandCorrelation()
        val id = commands.begin()
        assertFalse(commands.accept(JSONObject().put("requestId", "another-command").put("ok", true)))
        assertFalse(commands.accept(JSONObject().put("requestId", id).put("ok", "true")))
        assertFalse(commands.accept(JSONObject().put("ok", true)))
        val valid = JSONObject().put("requestId", id).put("ok", false)
        assertTrue(commands.accept(valid))
        assertFalse(commands.accept(valid))
    }

    @Test fun lateReplyAfterReconnectCannotCompleteTheNextCommand() {
        val commands = BleCommandCorrelation()
        val old = commands.begin()
        commands.clear()
        val current = commands.begin()
        assertNotEquals(old, current)
        assertFalse(commands.accept(JSONObject().put("requestId", old).put("ok", true)))
        assertTrue(commands.accept(JSONObject().put("requestId", current).put("ok", true)))
    }

    @Test fun overlappingSendIsRejectedInsteadOfReplacingThePendingCommand() {
        val commands = BleCommandCorrelation()
        val id = commands.begin()
        try { commands.begin(); fail("Pending command overwritten") }
        catch (_: IllegalStateException) { }
        assertTrue(commands.accept(JSONObject().put("requestId", id).put("reason", "ble_session_required"), error = true))
    }
}
