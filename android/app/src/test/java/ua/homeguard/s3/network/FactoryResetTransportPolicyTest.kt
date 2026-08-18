package ua.homeguard.s3.network

import org.junit.Assert.assertEquals
import org.junit.Test

class FactoryResetTransportPolicyTest {
    @Test
    fun connectionFailureBeforeRequestBodyIsSentIsRejected() {
        assertEquals(
            FactoryResetResult.REJECTED,
            classifyFactoryResetIOException(requestBodySent = false),
        )
    }

    @Test
    fun disconnectAfterRequestBodyWasSentIsExpectedResetLoss() {
        assertEquals(
            FactoryResetResult.CONNECTION_LOST,
            classifyFactoryResetIOException(requestBodySent = true),
        )
    }
}
