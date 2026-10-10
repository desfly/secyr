package ua.homeguard.s3.notifications

import org.junit.Assert.assertEquals
import org.junit.Test
import ua.homeguard.s3.model.SystemEventRecord
import ua.homeguard.s3.model.SystemMode

class AlertPolicyTest {
    private fun event(type: String, source: Int = 1) = SystemEventRecord(1, 1, type, source, 1)
    @Test fun disarmedZoneTamperIsStatus() {
        for (zone in 1..8) assertEquals(AlertSeverity.INFO,
            AlertPolicy.classify(event("TAMPER", zone), SystemMode.DISARMED)?.severity)
    }
    @Test fun explicitAlarmAndArmedTamperRemainCritical() {
        assertEquals(AlertSeverity.CRITICAL, AlertPolicy.classify(event("ALARM"), SystemMode.DISARMED)?.severity)
        assertEquals(AlertSeverity.CRITICAL, AlertPolicy.classify(event("TAMPER"), SystemMode.ARMED_AWAY)?.severity)
        assertEquals(AlertSeverity.CRITICAL, AlertPolicy.classify(event("TAMPER"))?.severity)
        assertEquals(AlertSeverity.CRITICAL, AlertPolicy.classify(event("TAMPER", 20), SystemMode.DISARMED)?.severity)
    }
}
