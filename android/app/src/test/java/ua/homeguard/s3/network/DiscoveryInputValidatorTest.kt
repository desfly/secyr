package ua.homeguard.s3.network

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class DiscoveryInputValidatorTest {
    @Test
    fun `accepts plain IPv4 and adds http`() {
        assertEquals("http://192.168.1.50", DiscoveryInputValidator.normalizeManualAddress("192.168.1.50"))
    }

    @Test
    fun `preserves explicit port`() {
        assertEquals("http://192.168.1.50:8080", DiscoveryInputValidator.normalizeManualAddress("192.168.1.50:8080"))
    }

    @Test
    fun `preserves https scheme`() {
        assertEquals("https://homeguard.local:8443", DiscoveryInputValidator.normalizeManualAddress("https://homeguard.local:8443/"))
    }

    @Test
    fun `rejects invalid ports paths queries and credentials`() {
        assertNull(DiscoveryInputValidator.normalizeManualAddress("192.168.1.50:70000"))
        assertNull(DiscoveryInputValidator.normalizeManualAddress("http://192.168.1.50/api"))
        assertNull(DiscoveryInputValidator.normalizeManualAddress("http://192.168.1.50?x=1"))
        assertNull(DiscoveryInputValidator.normalizeManualAddress("http://user@192.168.1.50"))
    }

    @Test
    fun `normalizes valid device IDs`() {
        assertEquals("HG-ACA7041DA710", DiscoveryInputValidator.normalizeDeviceId("  HG-ACA7041DA710  "))
        assertEquals("abc_123.test", DiscoveryInputValidator.normalizeDeviceId("abc_123.test"))
    }

    @Test
    fun `rejects malformed device IDs`() {
        assertNull(DiscoveryInputValidator.normalizeDeviceId("ab"))
        assertNull(DiscoveryInputValidator.normalizeDeviceId("bad id"))
        assertNull(DiscoveryInputValidator.normalizeDeviceId("bad/id"))
    }
}
