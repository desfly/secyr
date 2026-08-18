package ua.homeguard.s3.network

import org.junit.Assert.assertEquals
import org.junit.Test

class UdpDiscoveryCadenceTest {
    @Test
    fun emptyDiscoveryKeepsFastFiveSecondCadence() {
        assertEquals(5_000L, nextUdpDiscoveryDelayMs(false))
    }

    @Test
    fun foundControllerUsesLowerSteadyStateCadence() {
        assertEquals(15_000L, nextUdpDiscoveryDelayMs(true))
    }
}
