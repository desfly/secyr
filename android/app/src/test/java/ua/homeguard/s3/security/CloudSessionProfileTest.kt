package ua.homeguard.s3.security

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class CloudSessionProfileTest {
    @Test
    fun guestIsSensorOnlyAndCannotSubscribeFullState() {
        val profile = CloudSessionProfile.fromController(
            id = "guest1",
            name = "Guest",
            role = "guest",
            enabled = true,
            canArm = true,
            canDisarm = true,
            canControlValves = true,
            canManageUsers = true,
        )
        assertTrue(profile.authenticated)
        assertTrue(profile.sensorOnly)
        assertFalse(profile.canSubscribeFullState)
        assertFalse(profile.canArm)
        assertFalse(profile.canDisarm)
        assertFalse(profile.canControlValves)
        assertFalse(profile.canManageUsers)
    }

    @Test
    fun userCanMonitorAndControlButCannotManageUsers() {
        val profile = CloudSessionProfile.fromController(
            id = "user1",
            name = "User",
            role = "user",
            enabled = true,
            canArm = true,
            canDisarm = true,
            canControlValves = true,
            canManageUsers = true,
        )
        assertTrue(profile.canSubscribeFullState)
        assertTrue(profile.canArm)
        assertTrue(profile.canDisarm)
        assertTrue(profile.canControlValves)
        assertFalse(profile.canManageUsers)
    }

    @Test
    fun adminCanManageUsers() {
        val profile = CloudSessionProfile.fromController(
            id = "admin",
            name = "Admin",
            role = "admin",
            enabled = true,
            canArm = true,
            canDisarm = true,
            canControlValves = true,
            canManageUsers = true,
        )
        assertTrue(profile.canSubscribeFullState)
        assertTrue(profile.canManageUsers)
    }

    @Test
    fun disabledOrUnknownRoleIsLocked() {
        assertFalse(CloudSessionProfile.fromController("x", "X", "guest", false, false, false, false, false).authenticated)
        assertFalse(CloudSessionProfile.fromController("x", "X", "other", true, false, false, false, false).authenticated)
    }
}
