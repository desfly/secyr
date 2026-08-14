package ua.homeguard.s3.network

import org.junit.Assert.assertEquals
import org.junit.Test
import ua.homeguard.s3.model.ControlPath

class DiscoveryRouteTest {
    @Test
    fun matchingLocalDeviceUsesLocalRoute() {
        val path = selectControlPath(
            EndpointAvailability(
                hasDeviceId = true,
                matchingLocalFound = true,
                matchingLocalSecure = false,
                matchingLocalApiVersion = 1,
                hasLastKnownLocal = true,
                remoteAccessEnabled = true,
                hasCloudBaseUrl = true,
            )
        )
        assertEquals(ControlPath.LOCAL, path)
    }

    @Test
    fun unsupportedLocalApiFallsBackToLastKnownLocal() {
        val path = selectControlPath(
            EndpointAvailability(
                hasDeviceId = true,
                matchingLocalFound = true,
                matchingLocalSecure = false,
                matchingLocalApiVersion = 2,
                hasLastKnownLocal = true,
                remoteAccessEnabled = true,
                hasCloudBaseUrl = true,
            )
        )
        assertEquals(ControlPath.LAST_KNOWN_LOCAL, path)
    }

    @Test
    fun lastKnownLocalWinsBeforeCloud() {
        val path = selectControlPath(
            EndpointAvailability(
                hasDeviceId = true,
                matchingLocalFound = false,
                matchingLocalSecure = false,
                matchingLocalApiVersion = 0,
                hasLastKnownLocal = true,
                remoteAccessEnabled = true,
                hasCloudBaseUrl = true,
            )
        )
        assertEquals(ControlPath.LAST_KNOWN_LOCAL, path)
    }

    @Test
    fun cloudUsedWhenNoLocalAddressRemains() {
        val path = selectControlPath(
            EndpointAvailability(
                hasDeviceId = true,
                matchingLocalFound = false,
                matchingLocalSecure = false,
                matchingLocalApiVersion = 0,
                hasLastKnownLocal = false,
                remoteAccessEnabled = true,
                hasCloudBaseUrl = true,
            )
        )
        assertEquals(ControlPath.CLOUD, path)
    }

    @Test
    fun missingDeviceIdentityIsOffline() {
        val path = selectControlPath(
            EndpointAvailability(
                hasDeviceId = false,
                matchingLocalFound = false,
                matchingLocalSecure = false,
                matchingLocalApiVersion = 0,
                hasLastKnownLocal = false,
                remoteAccessEnabled = true,
                hasCloudBaseUrl = true,
            )
        )
        assertEquals(ControlPath.OFFLINE, path)
    }

    @Test
    fun cloudRequiresRemoteAccessAndBaseUrl() {
        val noRemote = selectControlPath(
            EndpointAvailability(true, false, false, 0, false, false, true)
        )
        val noCloudUrl = selectControlPath(
            EndpointAvailability(true, false, false, 0, false, true, false)
        )
        assertEquals(ControlPath.OFFLINE, noRemote)
        assertEquals(ControlPath.OFFLINE, noCloudUrl)
    }
}
