package ua.homeguard.s3.network

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ControllerIdentityTest {
    @Test
    fun `same host is one controller despite scheme port and temporary id`() {
        assertTrue(
            ControllerIdentity.sameController(
                leftDeviceId = "temp-http",
                leftBaseUrl = "http://192.168.55.40:80",
                rightDeviceId = "esp-real-id",
                rightBaseUrl = "https://192.168.55.40:443/",
            )
        )
    }

    @Test
    fun `manual ip registration reconciles with discovered controller across transport changes`() {
        assertTrue(
            ControllerIdentity.sameController(
                leftDeviceId = "manual-deadbeef",
                leftBaseUrl = "http://192.168.55.40:80/",
                rightDeviceId = "HG-ACA7041DA710",
                rightBaseUrl = "https://192.168.55.40:8443/api/v1",
            )
        )
    }

    @Test
    fun `different hosts are not merged when ids differ`() {
        assertFalse(
            ControllerIdentity.sameController(
                leftDeviceId = "esp-a",
                leftBaseUrl = "http://192.168.55.40:80",
                rightDeviceId = "esp-b",
                rightBaseUrl = "http://192.168.55.41:80",
            )
        )
    }

    @Test
    fun `same stable device id remains one controller after address change`() {
        assertTrue(
            ControllerIdentity.sameController(
                leftDeviceId = "esp-stable",
                leftBaseUrl = "http://192.168.55.40:80",
                rightDeviceId = "esp-stable",
                rightBaseUrl = "http://192.168.55.77:80",
            )
        )
    }

    @Test
    fun `stable device id matching ignores case and surrounding whitespace`() {
        assertTrue(
            ControllerIdentity.sameController(
                leftDeviceId = " HG-A1B2C3 ",
                leftBaseUrl = "http://192.168.55.40:80",
                rightDeviceId = "hg-a1b2c3",
                rightBaseUrl = "http://192.168.55.77:80",
            )
        )
    }

    @Test
    fun `ipv6 host normalization ignores brackets and port`() {
        assertEquals("fe80::1234", ControllerIdentity.hostFromBaseUrl("http://[fe80::1234]:8080/api"))
        assertEquals("host:fe80::1234", ControllerIdentity.key("ignored", "[FE80::1234]"))
    }
}
