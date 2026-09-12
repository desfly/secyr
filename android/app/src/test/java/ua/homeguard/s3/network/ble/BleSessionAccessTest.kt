package ua.homeguard.s3.network.ble

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class BleSessionAccessTest {
    @Test
    fun `known security commands follow advertised capabilities`() {
        val access = BleSessionAccess(
            actor = "operator-1",
            role = "operator",
            monitor = true,
            armHome = true,
            armAway = false,
            disarm = true,
            panic = false,
        )

        assertTrue(access.authenticated)
        assertTrue(access.allows("security.arm_home"))
        assertFalse(access.allows("security.arm_away"))
        assertTrue(access.allows("security.disarm"))
        assertFalse(access.allows("security.panic"))
    }

    @Test
    fun `valve and administration commands follow advertised capabilities`() {
        val access = BleSessionAccess(
            actor = "service-1",
            role = "service",
            valves = true,
            networkConfigure = false,
            accessManage = false,
            serviceInvalidate = true,
        )

        assertTrue(access.allows("valve.open"))
        assertTrue(access.allows("valve.close"))
        assertFalse(access.allows("network.configure"))
        assertFalse(access.allows("access.manage"))
        assertTrue(access.allows("system.service.invalidate"))
    }

    @Test
    fun `unknown command remains server-authoritative`() {
        val access = BleSessionAccess(actor = "admin-1", role = "admin")
        assertTrue(access.allows("output.control"))
        assertTrue(access.allows("future.command"))
    }

    @Test
    fun `empty actor is not an authenticated access record`() {
        assertFalse(BleSessionAccess(role = "admin").authenticated)
    }
}
