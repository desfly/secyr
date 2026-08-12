package ua.homeguard.s3.model

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AccessModelsTest {
    @Test
    fun adminAllowsEveryCurrentCommandType() {
        val session = AccessSession(
            actor = "admin",
            name = "Administrator",
            role = AccessRole.ADMIN,
            capabilities = AccessCapabilities(),
        )

        CommandType.entries.forEach { command ->
            assertTrue("Admin must allow $command", session.allows(command))
        }
    }

    @Test
    fun userAllowsOnlyArmDisarmAndValvesWhenBackendCapabilitiesPermitThem() {
        val session = AccessSession(
            actor = "user",
            name = "Operator",
            role = AccessRole.USER,
            capabilities = AccessCapabilities(
                monitor = true,
                armHome = true,
                armAway = true,
                disarm = true,
                valves = true,
            ),
        )

        assertTrue(session.allows(CommandType.ARM_HOME))
        assertTrue(session.allows(CommandType.ARM_AWAY))
        assertTrue(session.allows(CommandType.DISARM))
        assertTrue(session.allows(CommandType.OPEN_VALVES))
        assertTrue(session.allows(CommandType.CLOSE_VALVES))

        assertFalse(session.allows(CommandType.SILENCE))
        assertFalse(session.allows(CommandType.RESET_ALARM))
        assertFalse(session.allows(CommandType.ENTER_MAINTENANCE))
        assertFalse(session.allows(CommandType.EXIT_MAINTENANCE))
    }

    @Test
    fun userCannotGainControlWhenBackendCapabilitiesAreFalse() {
        val session = AccessSession(
            actor = "user",
            name = "Restricted Operator",
            role = AccessRole.USER,
            capabilities = AccessCapabilities(monitor = true),
        )

        CommandType.entries.forEach { command ->
            assertFalse("Restricted User must deny $command", session.allows(command))
        }
    }

    @Test
    fun guestIsMonitoringOnlyByDefault() {
        val session = AccessSession(
            actor = "guest",
            name = "Guest",
            role = AccessRole.GUEST,
            capabilities = AccessCapabilities(monitor = true),
        )

        assertTrue(session.capabilities.monitor)
        CommandType.entries.forEach { command ->
            assertFalse("Guest must deny $command", session.allows(command))
        }
    }

    @Test
    fun guestDoesNotEscalateFromUnexpectedOperatorCapabilities() {
        val session = AccessSession(
            actor = "guest",
            name = "Guest",
            role = AccessRole.GUEST,
            capabilities = AccessCapabilities(
                monitor = true,
                armHome = true,
                armAway = true,
                disarm = true,
                valves = true,
            ),
        )

        // Defense in depth: a malformed backend response must not turn Guest
        // into an operator. Guest remains UI-control-disabled unconditionally.
        CommandType.entries.forEach { command ->
            assertFalse("Guest must never allow $command", session.allows(command))
        }
    }
}
