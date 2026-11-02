package ua.homeguard.s3.network.mqtt

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class MqttHealthPolicyTest {
    private val now = 1_000_000L

    @Test fun requiresBothFreshHeartbeatAndState() {
        assertTrue(MqttHealthPolicy.ready(now - 5_000L, now - 5_000L, "online", now))
        assertFalse(MqttHealthPolicy.ready(0L, now, "online", now))
        assertFalse(MqttHealthPolicy.ready(now, 0L, "online", now))
        assertFalse(MqttHealthPolicy.ready(now, now, "offline", now))
    }

    @Test fun fourMissedFiveSecondHeartbeatsExpireReadiness() {
        // State can still arrive even when the heartbeat path has stalled.
        assertTrue(MqttHealthPolicy.ready(now - 20_000L, now - 1_000L, "online", now))
        assertFalse(MqttHealthPolicy.ready(now - 20_001L, now - 1_000L, "online", now))
        assertFalse(MqttHealthPolicy.ready(now - 185_000L, now, "online", now))
    }

    @Test fun staleStateOrClockDiscontinuityCannotClaimHealthyLink() {
        assertTrue(MqttHealthPolicy.ready(now - 1_000L, now - 15_000L, "online", now))
        assertFalse(MqttHealthPolicy.ready(now - 1_000L, now - 15_001L, "online", now))
        assertFalse(MqttHealthPolicy.ready(now + 1L, now, "online", now))
        assertFalse(MqttHealthPolicy.ready(now, now + 1L, "online", now))
    }
}
