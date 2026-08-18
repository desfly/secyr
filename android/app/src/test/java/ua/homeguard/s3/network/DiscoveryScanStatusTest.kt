package ua.homeguard.s3.network

import org.junit.Assert.assertEquals
import org.junit.Assert.assertTrue
import org.junit.Test
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.DiscoverySource

class DiscoveryScanStatusTest {
    @Test
    fun manualRescanStaysScanningUntilBothBranchesFinish() {
        val status = combineDiscoveryScanStatus(
            udp = UdpDeviceDiscovery.ScanStatus(phase = "done", progress = 1f, accepted = 1),
            http = HttpSubnetDiscovery.ScanStatus(phase = "probing", progress = 0.5f, found = 1),
            manualRescanActive = true,
        )

        assertEquals("scanning", status.phase)
        assertTrue(status.progress in 0.74f..0.76f)
        assertEquals(2, status.accepted)
    }

    @Test
    fun completedManualRescanReportsDoneAtOneHundredPercent() {
        val status = combineDiscoveryScanStatus(
            udp = UdpDeviceDiscovery.ScanStatus(phase = "done", progress = 1f),
            http = HttpSubnetDiscovery.ScanStatus(phase = "done", progress = 1f),
            manualRescanActive = false,
        )

        assertEquals("done", status.phase)
        assertEquals(1f, status.progress)
    }

    @Test
    fun httpFailureIsVisibleAfterManualScanStops() {
        val status = combineDiscoveryScanStatus(
            udp = UdpDeviceDiscovery.ScanStatus(phase = "done", progress = 1f),
            http = HttpSubnetDiscovery.ScanStatus(
                phase = "error",
                progress = 1f,
                error = "Wi-Fi network unavailable",
            ),
            manualRescanActive = false,
        )

        assertEquals("error", status.phase)
        assertEquals(1f, status.progress)
        assertTrue(status.error.contains("Wi-Fi network unavailable"))
    }

    @Test
    fun backgroundUdpScanKeepsNativeUdpProgress() {
        val status = combineDiscoveryScanStatus(
            udp = UdpDeviceDiscovery.ScanStatus(phase = "listening", progress = 0.42f),
            http = HttpSubnetDiscovery.ScanStatus(phase = "done", progress = 1f),
            manualRescanActive = false,
        )

        assertEquals("listening", status.phase)
        assertEquals(0.42f, status.progress)
    }

    @Test
    fun staleDiscoveryReportsAreDroppedBeforeDeviceListDeduplication() {
        val now = 100_000L
        val fresh = DiscoveredDevice(
            deviceId = "HG-S3-FRESH",
            serviceName = "fresh",
            host = "192.168.1.20",
            port = 443,
            secure = true,
            source = DiscoverySource.MDNS,
            seenAtMs = now - DISCOVERY_FRESHNESS_MS,
        )
        val stale = fresh.copy(
            deviceId = "HG-S3-STALE",
            serviceName = "stale",
            host = "192.168.1.21",
            seenAtMs = now - DISCOVERY_FRESHNESS_MS - 1,
        )

        val result = freshDiscoveryReports(listOf(fresh, stale), now)

        assertEquals(listOf("HG-S3-FRESH"), result.map { it.deviceId })
    }
}
