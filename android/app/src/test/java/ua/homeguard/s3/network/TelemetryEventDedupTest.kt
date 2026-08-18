package ua.homeguard.s3.network

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import ua.homeguard.s3.model.SystemEventRecord

class TelemetryEventDedupTest {
    @Test
    fun duplicateSequenceIsNotReemitted() {
        val existing = listOf(event(7), event(6))
        val (updated, added) = mergeTelemetryEvent(existing, event(7))

        assertFalse(added)
        assertSame(existing, updated)
    }

    @Test
    fun newEventIsPrependedAndHistoryIsBounded() {
        val existing = listOf(event(7), event(6))
        val (updated, added) = mergeTelemetryEvent(existing, event(8), maxEvents = 2)

        assertTrue(added)
        assertEquals(listOf(8L, 7L), updated.map { it.sequence })
    }

    private fun event(sequence: Long) = SystemEventRecord(
        sequence = sequence,
        timestampMs = sequence * 1_000L,
        event = "alarm",
        sourceId = 1,
        value = 1,
    )
}
