package ua.homeguard.s3.events

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import ua.homeguard.s3.model.SystemEventRecord

class EventLogFilterTest {
    private val tamper = SystemEventRecord(
        sequence = 10,
        timestampMs = 1000,
        event = "input.changed",
        sourceId = 0,
        value = 1,
    )
    private val powerFail = SystemEventRecord(
        sequence = 11,
        timestampMs = 1100,
        event = "input.changed",
        sourceId = 1,
        value = 0,
    )

    @Test
    fun physicalInputsStayNeutralUntilPolarityIsKnown() {
        assertEquals(EventLogCategory.OTHER, EventLogFilterEngine.categoryOf(tamper))
        assertEquals(EventLogCategory.OTHER, EventLogFilterEngine.categoryOf(powerFail))
    }

    @Test
    fun searchUsesVisiblePhysicalInputNamesAndLevels() {
        val events = listOf(tamper, powerFail)
        assertEquals(listOf(tamper), EventLogFilterEngine.apply(events, EventLogFilter(query = "tamper")))
        assertEquals(listOf(powerFail), EventLogFilterEngine.apply(events, EventLogFilter(query = "power fail")))
        assertTrue(EventLogFilterEngine.apply(events, EventLogFilter(query = "high")).contains(tamper))
        assertTrue(EventLogFilterEngine.apply(events, EventLogFilter(query = "low")).contains(powerFail))
    }
}
