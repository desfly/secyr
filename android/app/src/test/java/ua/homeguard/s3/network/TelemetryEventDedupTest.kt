package ua.homeguard.s3.network

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertSame
import org.junit.Assert.assertTrue
import org.junit.Test
import ua.homeguard.s3.model.SystemEventRecord

class TelemetryEventDedupTest {
    @Test
    fun duplicateSequenceFromSameControllerIsNotReemitted() {
        val existing = listOf(event(7, "HG-A"), event(6, "HG-A"))
        val (updated, added) = mergeTelemetryEvent(existing, event(7, "hg-a"))

        assertFalse(added)
        assertSame(existing, updated)
    }

    @Test
    fun sameSequenceFromDifferentControllerIsKept() {
        val existing = listOf(event(7, "HG-A"))
        val (updated, added) = mergeTelemetryEvent(existing, event(7, "HG-B"))

        assertTrue(added)
        assertEquals(listOf("HG-B", "HG-A"), updated.map { it.controllerId })
    }

    @Test
    fun newEventIsPrependedAndHistoryIsBounded() {
        val existing = listOf(event(7, "HG-A"), event(6, "HG-A"))
        val (updated, added) = mergeTelemetryEvent(existing, event(8, "HG-A"), maxEvents = 2)

        assertTrue(added)
        assertEquals(listOf(8L, 7L), updated.map { it.sequence })
    }

    private fun event(sequence: Long, controllerId: String) = SystemEventRecord(
        sequence = sequence,
        timestampMs = sequence * 1_000L,
        event = "alarm",
        sourceId = 1,
        value = 1,
        controllerId = controllerId,
    )
}
