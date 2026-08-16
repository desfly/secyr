package ua.homeguard.s3.model

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class AccessCapabilitiesTest {
    private val operatorCapabilities = AccessCapabilities(
        monitor = true,
        armHome = true,
        armAway = true,
        disarm = true,
        panic = false,
        valves = true,
        networkConfigure = false,
        accessManage = false,
        serviceInvalidate = false,
    )

    @Test
    fun adminStillRespectsRuntimeCapabilityWiring() {
        val admin = AccessSession(
            actor = "admin",
            name = "Admin",
            role = AccessRole.ADMIN,
            capabilities = operatorCapabilities,
        )

        assertTrue(admin.allows(CommandType.ARM_HOME))
        assertTrue(admin.allows(CommandType.ARM_AWAY))
        assertTrue(admin.allows(CommandType.DISARM))
        assertTrue(admin.allows(CommandType.OPEN_VALVES))
        assertTrue(admin.allows(CommandType.CLOSE_VALVES))

        assertFalse(admin.allows(CommandType.SILENCE))
        assertFalse(admin.allows(CommandType.RESET_ALARM))
        assertFalse(admin.allows(CommandType.ENTER_MAINTENANCE))
        assertFalse(admin.allows(CommandType.EXIT_MAINTENANCE))
    }

    @Test
    fun userGetsOnlyExplicitOperatorCapabilities() {
        val user = AccessSession(
            actor = "user1",
            name = "User",
            role = AccessRole.USER,
            capabilities = operatorCapabilities,
        )

        assertTrue(user.allows(CommandType.ARM_HOME))
        assertTrue(user.allows(CommandType.ARM_AWAY))
        assertTrue(user.allows(CommandType.DISARM))
        assertTrue(user.allows(CommandType.OPEN_VALVES))
        assertTrue(user.allows(CommandType.CLOSE_VALVES))
        assertFalse(user.allows(CommandType.SILENCE))
        assertFalse(user.allows(CommandType.RESET_ALARM))
    }

    @Test
    fun guestCannotIssueOperatorCommands() {
        val guest = AccessSession(
            actor = "guest",
            name = "Guest",
            role = AccessRole.GUEST,
            capabilities = operatorCapabilities,
        )

        CommandType.entries.forEach { command ->
            assertFalse("guest unexpectedly allowed $command", guest.allows(command))
        }
    }
}
