package ua.homeguard.s3.network

import java.io.StringReader
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class HttpDiscoveryBodyLimitTest {
    @Test
    fun bodyAtLimitIsAccepted() {
        assertEquals("1234", readBoundedDiscoveryText(StringReader("1234"), maxChars = 4))
    }

    @Test
    fun bodyOverLimitIsRejected() {
        assertNull(readBoundedDiscoveryText(StringReader("12345"), maxChars = 4))
    }

    @Test
    fun emptyBodyIsAcceptedWithoutAllocationGrowth() {
        assertEquals("", readBoundedDiscoveryText(StringReader(""), maxChars = 4))
    }
}
