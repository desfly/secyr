package ua.homeguard.s3.storage

import org.junit.Assert.assertEquals
import org.junit.Assert.assertSame
import org.junit.Test
import ua.homeguard.s3.model.SystemEventRecord

class EventHistoryMergeTest {
    private fun event(sequence: Long) = SystemEventRecord(
        sequence = sequence,
        timestampMs = sequence * 10,
        event = "test",
        sourceId = 1,
        value = sequence.toInt(),
    )

    @Test
    fun duplicateSequenceReturnsSameListWithoutWriteWork() {
        val current = listOf(event(2), event(1))
        val merged = mergeEventHistory(current, event(2))

        assertSame(current, merged)
    }

    @Test
    fun newEventsStayNewestFirstAndBounded() {
        val current = listOf(event(2), event(1))
        val merged = mergeEventHistory(current, event(3), maxEvents = 2)

        assertEquals(listOf(3L, 2L), merged.map { it.sequence })
    }
}
