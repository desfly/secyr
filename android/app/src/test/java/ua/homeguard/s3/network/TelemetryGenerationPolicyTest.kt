package ua.homeguard.s3.network

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class TelemetryGenerationPolicyTest {
    @Test
    fun onlyCurrentGenerationIsAccepted() {
        assertTrue(isCurrentTelemetryGeneration(7L, 7L))
        assertFalse(isCurrentTelemetryGeneration(6L, 7L))
        assertFalse(isCurrentTelemetryGeneration(8L, 7L))
    }
}
