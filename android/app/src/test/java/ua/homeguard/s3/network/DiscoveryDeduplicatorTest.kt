package ua.homeguard.s3.network

import org.junit.Assert.assertEquals
import org.junit.Test
import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.DiscoverySource

class DiscoveryDeduplicatorTest {
    @Test
    fun sameStableIdAcrossDifferentHostsIsOneController() {
        val collapsed = DiscoveryDeduplicator.collapse(
            listOf(
                device("HG-1", "192.168.1.10", 80, DiscoverySource.UDP, 100),
                device("hg-1", "homeguard.local", 80, DiscoverySource.MDNS, 200),
            ),
        )

        assertEquals(1, collapsed.size)
        assertEquals("homeguard.local", collapsed.single().host)
    }

    @Test
    fun sameHostAcrossDifferentTransportIdsIsOneController() {
        val collapsed = DiscoveryDeduplicator.collapse(
            listOf(
                device("temporary-mdns", "192.168.1.20", 80, DiscoverySource.MDNS, 100),
                device("stable-20", "192.168.1.20", 8080, DiscoverySource.HTTP, 200),
            ),
        )

        assertEquals(1, collapsed.size)
        assertEquals("stable-20", collapsed.single().deviceId)
    }

    @Test
    fun identityReconciliationIsTransitive() {
        val collapsed = DiscoveryDeduplicator.collapse(
            listOf(
                device("stable-a", "192.168.1.30", 80, DiscoverySource.UDP, 100),
                device("stable-a", "192.168.1.31", 80, DiscoverySource.MDNS, 200),
                device("temporary-c", "192.168.1.31", 8080, DiscoverySource.HTTP, 300),
            ),
        )

        assertEquals(1, collapsed.size)
        assertEquals("temporary-c", collapsed.single().deviceId)
    }

    @Test
    fun unrelatedControllersRemainSeparate() {
        val collapsed = DiscoveryDeduplicator.collapse(
            listOf(
                device("stable-a", "192.168.1.40", 80, DiscoverySource.UDP, 100),
                device("stable-b", "192.168.1.41", 80, DiscoverySource.UDP, 100),
            ),
        )

        assertEquals(2, collapsed.size)
    }

    @Test
    fun mdnsWinsWhenTimestampIsEqual() {
        val collapsed = DiscoveryDeduplicator.collapse(
            listOf(
                device("stable-a", "192.168.1.50", 80, DiscoverySource.HTTP, 100),
                device("stable-a", "192.168.1.50", 80, DiscoverySource.UDP, 100),
                device("stable-a", "192.168.1.50", 80, DiscoverySource.MDNS, 100),
            ),
        )

        assertEquals(DiscoverySource.MDNS, collapsed.single().source)
    }

    private fun device(
        id: String,
        host: String,
        port: Int,
        source: DiscoverySource,
        seenAt: Long,
    ) = DiscoveredDevice(
        deviceId = id,
        serviceName = id,
        host = host,
        port = port,
        secure = false,
        source = source,
        seenAtMs = seenAt,
    )
}
