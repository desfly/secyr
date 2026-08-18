package ua.homeguard.s3.network

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class FactoryResetRunGateTest {
    @Test
    fun `second destructive reset is rejected until first run releases`() {
        val gate = FactoryResetRunGate()

        assertTrue(gate.tryAcquire())
        assertFalse(gate.tryAcquire())

        gate.release()
        assertTrue(gate.tryAcquire())
    }
}
