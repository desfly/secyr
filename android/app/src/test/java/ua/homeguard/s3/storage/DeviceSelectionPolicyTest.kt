package ua.homeguard.s3.storage

import org.junit.Assert.assertEquals
import org.junit.Test

class DeviceSelectionPolicyTest {
    @Test
    fun `switching device without confirmed URL clears stale local route and certificate`() {
        val result = DeviceSelectionPolicy.select(
            currentDeviceId = "HG-OLD",
            currentLocalUrl = "https://192.168.1.10:8443/",
            currentCertificateSha256 = "OLD-CERT",
            nextDeviceId = "HG-NEW",
        )

        assertEquals("HG-NEW", result.deviceId)
        assertEquals("", result.lastKnownLocalUrl)
        assertEquals("", result.localCertificateSha256)
    }

    @Test
    fun `switching device with confirmed URL uses new URL but never carries old certificate`() {
        val result = DeviceSelectionPolicy.select(
            currentDeviceId = "HG-OLD",
            currentLocalUrl = "https://192.168.1.10:8443",
            currentCertificateSha256 = "OLD-CERT",
            nextDeviceId = "HG-NEW",
            confirmedLocalUrl = "http://192.168.1.20:80/",
        )

        assertEquals("HG-NEW", result.deviceId)
        assertEquals("http://192.168.1.20:80", result.lastKnownLocalUrl)
        assertEquals("", result.localCertificateSha256)
    }

    @Test
    fun `selecting same device preserves known route and certificate when no new URL is supplied`() {
        val result = DeviceSelectionPolicy.select(
            currentDeviceId = "HG-1",
            currentLocalUrl = "https://192.168.1.30:8443/",
            currentCertificateSha256 = "CERT",
            nextDeviceId = "HG-1",
        )

        assertEquals("HG-1", result.deviceId)
        assertEquals("https://192.168.1.30:8443", result.lastKnownLocalUrl)
        assertEquals("CERT", result.localCertificateSha256)
    }

    @Test
    fun `same device id with different case preserves route and certificate`() {
        val result = DeviceSelectionPolicy.select(
            currentDeviceId = "HG-AbC123",
            currentLocalUrl = "https://192.168.1.31:8443/",
            currentCertificateSha256 = "CERT",
            nextDeviceId = "hg-abc123",
        )

        assertEquals("hg-abc123", result.deviceId)
        assertEquals("https://192.168.1.31:8443", result.lastKnownLocalUrl)
        assertEquals("CERT", result.localCertificateSha256)
    }

    @Test
    fun `confirmed route is normalized by trimming spaces and trailing slash`() {
        val result = DeviceSelectionPolicy.select(
            currentDeviceId = "",
            currentLocalUrl = "",
            currentCertificateSha256 = "",
            nextDeviceId = "HG-1",
            confirmedLocalUrl = "  http://192.168.1.40:8080/  ",
        )

        assertEquals("http://192.168.1.40:8080", result.lastKnownLocalUrl)
    }
}
