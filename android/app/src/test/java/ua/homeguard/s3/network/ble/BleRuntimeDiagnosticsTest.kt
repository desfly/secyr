package ua.homeguard.s3.network.ble

import org.junit.After
import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Before
import org.junit.Test

class BleRuntimeDiagnosticsTest {
    @Before
    fun setUp() {
        BleRuntimeDiagnostics.reset()
    }

    @After
    fun tearDown() {
        BleRuntimeDiagnostics.reset()
    }

    @Test
    fun keepsOrderedBoundedTransitionHistory() {
        for (index in 1..14) {
            BleRuntimeDiagnostics.update(
                stage = "S$index",
                detail = "detail-$index",
                statusCode = if (index == 14) 22 else null,
                address = "AC:A7:04:1D:A7:12",
            )
        }

        val diagnostic = BleRuntimeDiagnostics.current()
        assertEquals(14L, diagnostic.transition)
        assertEquals(12, diagnostic.history.size)
        assertEquals("S3", diagnostic.history.first().stage)
        assertEquals("S14", diagnostic.history.last().stage)
        assertEquals(22, diagnostic.history.last().statusCode)
        assertEquals("AC:A7:04:1D:A7:12", diagnostic.address)
    }

    @Test
    fun resetClearsCurrentStageAndHistory() {
        BleRuntimeDiagnostics.update("CONNECTING", "test")
        BleRuntimeDiagnostics.update("DISCOVERING", "test")

        BleRuntimeDiagnostics.reset()

        val diagnostic = BleRuntimeDiagnostics.current()
        assertEquals("IDLE", diagnostic.stage)
        assertEquals(0L, diagnostic.transition)
        assertTrue(diagnostic.history.isEmpty())
    }
}
