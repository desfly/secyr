package ua.homeguard.s3.storage

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.DiscoverySource

class RegisteredDeviceRefreshPolicyTest {
    private val previous = RegisteredDevice(
        deviceId = "HG-001",
        name = "Garage",
        baseUrl = "https://192.168.1.50:443",
        lastSeenAtMs = 1_000L,
    )

    @Test
    fun timestampOnlyRefreshIsThrottledUntilPersistenceInterval() {
        assertFalse(shouldPersistDiscoveredRefresh(previous, discovered(seenAtMs = 60_999L)))
        assertTrue(shouldPersistDiscoveredRefresh(previous, discovered(seenAtMs = 61_000L)))
    }

    @Test
    fun endpointChangePersistsImmediately() {
        val moved = discovered(host = "192.168.1.51", seenAtMs = 2_000L)
        assertTrue(shouldPersistDiscoveredRefresh(previous, moved))
    }

    @Test
    fun stableIdCaseDifferenceDoesNotForcePersistence() {
        val same = discovered(deviceId = "hg-001", seenAtMs = 2_000L)
        assertFalse(shouldPersistDiscoveredRefresh(previous, same))
    }

    private fun discovered(
        deviceId: String = "HG-001",
        host: String = "192.168.1.50",
        seenAtMs: Long,
    ) = DiscoveredDevice(
        deviceId = deviceId,
        serviceName = deviceId,
        host = host,
        port = 443,
        secure = true,
        source = DiscoverySource.UDP,
        seenAtMs = seenAtMs,
    )
}
