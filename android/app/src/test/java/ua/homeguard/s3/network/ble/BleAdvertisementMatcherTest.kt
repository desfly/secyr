package ua.homeguard.s3.network.ble

import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class BleAdvertisementMatcherTest {
    @Test
    fun acceptsOnlyMatchingControllerWithHomeGuardService() {
        assertTrue(
            BleAdvertisementMatcher.matches(
                deviceId = "HG-ACA7041DA710",
                advertisedName = "HomeGuard-S3-1DA710",
                cachedName = "",
                serviceMatches = true,
            )
        )
    }

    @Test
    fun rejectsAnotherHomeGuardEvenWhenServiceMatches() {
        assertFalse(
            BleAdvertisementMatcher.matches(
                deviceId = "HG-ACA7041DA710",
                advertisedName = "HomeGuard-S3-FFFFFF",
                cachedName = "",
                serviceMatches = true,
            )
        )
    }

    @Test
    fun rejectsGenericNameAndNameWithoutService() {
        assertFalse(
            BleAdvertisementMatcher.matches(
                deviceId = "HG-ACA7041DA710",
                advertisedName = "HomeGuard-S3",
                cachedName = "",
                serviceMatches = true,
            )
        )
        assertFalse(
            BleAdvertisementMatcher.matches(
                deviceId = "HG-ACA7041DA710",
                advertisedName = "HomeGuard-S3-1DA710",
                cachedName = "",
                serviceMatches = false,
            )
        )
    }
}
