package ua.homeguard.s3.ui.screens

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotEquals
import org.junit.Test
import ua.homeguard.s3.model.SystemEventRecord

class DashboardEventScopeTest {
    @Test
    fun selectedControllerKeepsOwnAndLegacyEventsOnly() {
        val events = listOf(
            event(10, "HG-A"),
            event(10, "HG-B"),
            event(9, ""),
        )

        val selected = eventsForController(events, "hg-a")

        assertEquals(listOf("HG-A", ""), selected.map { it.controllerId })
    }

    @Test
    fun composeKeysDifferForSameSequenceFromDifferentControllers() {
        assertNotEquals(eventListKey(event(42, "HG-A")), eventListKey(event(42, "HG-B")))
        assertEquals(eventListKey(event(42, "HG-A")), eventListKey(event(42, "hg-a")))
    }

    private fun event(sequence: Long, controllerId: String) = SystemEventRecord(
        sequence = sequence,
        timestampMs = sequence * 100L,
        event = "alarm",
        controllerId = controllerId,
    )
}
