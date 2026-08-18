package ua.homeguard.s3.repository

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ProvisioningRunGateTest {
    @Test
    fun `second acquire is rejected until active run releases`() {
        val gate = ProvisioningRunGate()

        assertTrue(gate.tryAcquire())
        assertFalse(gate.tryAcquire())

        gate.release()
        assertTrue(gate.tryAcquire())
    }
}
