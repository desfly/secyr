package ua.homeguard.s3.network

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceEndpoint

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

    @Test
    fun disabledAutoReconnectBlocksRetryForOtherwiseValidTarget() {
        val endpoint = DeviceEndpoint(
            deviceId = "HG-1",
            apiBaseUrl = "http://192.168.1.20",
            websocketUrl = "ws://192.168.1.20/ws",
            path = ControlPath.LOCAL,
        )

        assertFalse(reconnectAllowed(false, endpoint, "token"))
        assertTrue(reconnectAllowed(true, endpoint, "token"))
    }

    @Test
    fun reconnectStillRequiresConnectableEndpointAndToken() {
        val offline = DeviceEndpoint("HG-1", "", "", ControlPath.OFFLINE)
        val local = DeviceEndpoint("HG-1", "http://192.168.1.20", "ws://192.168.1.20/ws", ControlPath.LOCAL)

        assertFalse(reconnectAllowed(true, offline, "token"))
        assertFalse(reconnectAllowed(true, local, ""))
    }
}
