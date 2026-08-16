package ua.homeguard.s3.model

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class DeviceIdentityTest {
    @Test
    fun friendlyNameRejectsGeneratedAndTechnicalValues() {
        assertTrue(DeviceIdentity.isForbiddenFriendlyName(""))
        assertTrue(DeviceIdentity.isForbiddenFriendlyName("   "))
        assertTrue(DeviceIdentity.isForbiddenFriendlyName("HomeGuard"))
        assertTrue(DeviceIdentity.isForbiddenFriendlyName("homeguard-s3"))

        val id = "HG-ACA7041DA710"
        val endpoint = "http://192.168.55.253:80"
        assertTrue(DeviceIdentity.isForbiddenFriendlyName(id, id, endpoint))
        assertTrue(DeviceIdentity.isForbiddenFriendlyName(endpoint, id, endpoint))
        assertTrue(DeviceIdentity.isForbiddenFriendlyName("192.168.55.253", id, endpoint))

        assertFalse(DeviceIdentity.isForbiddenFriendlyName("Будинок", id, endpoint))
        assertFalse(DeviceIdentity.isForbiddenFriendlyName("Гараж", id, endpoint))
    }

    @Test
    fun canonicalHardwareIdMatchesDifferentFormatting() {
        assertTrue(
            DeviceIdentity.samePhysicalDevice(
                "HG-ACA7041DA710",
                "",
                "ac:a7:04:1d:a7:10",
                "",
            ),
        )
    }

    @Test
    fun manualAndDiscoveredRecordsMatchByEndpointHost() {
        assertTrue(
            DeviceIdentity.samePhysicalDevice(
                "manual-1234",
                "http://192.168.55.253:80",
                "HG-ACA7041DA710",
                "http://192.168.55.253",
            ),
        )
    }

    @Test
    fun differentControllersDoNotCollapse() {
        assertFalse(
            DeviceIdentity.samePhysicalDevice(
                "HG-ACA7041DA710",
                "http://192.168.55.253",
                "HG-ACA7041DA711",
                "http://192.168.55.254",
            ),
        )
    }
}
