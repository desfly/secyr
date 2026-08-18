package ua.homeguard.s3.network

import org.junit.Assert.assertEquals
import org.junit.Test

class DeviceSessionReconnectPolicyTest {
    @Test
    fun reconnectBackoffStartsFastAndCapsAtThirtySeconds() {
        assertEquals(2_000L, reconnectDelayMs(0))
        assertEquals(5_000L, reconnectDelayMs(1))
        assertEquals(10_000L, reconnectDelayMs(2))
        assertEquals(20_000L, reconnectDelayMs(3))
        assertEquals(30_000L, reconnectDelayMs(4))
        assertEquals(30_000L, reconnectDelayMs(99))
    }

    @Test
    fun negativeAttemptUsesFirstDelay() {
        assertEquals(2_000L, reconnectDelayMs(-1))
    }
}
