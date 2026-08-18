package ua.homeguard.s3.network

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class NsdGenerationPolicyTest {
    @Test
    fun `callback from active generation is accepted`() {
        assertTrue(isDiscoveryGenerationActive(started = true, activeGeneration = 7L, callbackGeneration = 7L))
    }

    @Test
    fun `callback from previous generation is rejected after restart`() {
        assertFalse(isDiscoveryGenerationActive(started = true, activeGeneration = 8L, callbackGeneration = 7L))
    }

    @Test
    fun `callback is rejected while discovery is stopped`() {
        assertFalse(isDiscoveryGenerationActive(started = false, activeGeneration = 7L, callbackGeneration = 7L))
    }
}
