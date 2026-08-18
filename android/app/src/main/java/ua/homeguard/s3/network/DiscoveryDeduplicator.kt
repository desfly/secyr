package ua.homeguard.s3.network

import ua.homeguard.s3.model.DiscoveredDevice
import ua.homeguard.s3.model.DiscoverySource

/**
 * Collapses discovery reports to one entry per physical controller.
 *
 * The relation is intentionally transitive: if A and B share a stable device ID,
 * and B and C share the same normalized endpoint host, then A/B/C are one physical
 * controller even when A and C do not match directly. This prevents one ESP from
 * appearing as multiple cards when mDNS/UDP/HTTP observe different temporary
 * addresses or identifiers during the same scan window.
 */
object DiscoveryDeduplicator {
    fun collapse(devices: List<DiscoveredDevice>): List<DiscoveredDevice> {
        if (devices.size < 2) return devices.sortedBy { it.deviceId.lowercase() }

        val parent = IntArray(devices.size) { it }

        fun find(value: Int): Int {
            var node = value
            while (parent[node] != node) {
                parent[node] = parent[parent[node]]
                node = parent[node]
            }
            return node
        }

        fun union(left: Int, right: Int) {
            val leftRoot = find(left)
            val rightRoot = find(right)
            if (leftRoot != rightRoot) parent[rightRoot] = leftRoot
        }

        for (left in devices.indices) {
            for (right in (left + 1) until devices.size) {
                val a = devices[left]
                val b = devices[right]
                if (ControllerIdentity.sameController(a.deviceId, a.baseUrl, b.deviceId, b.baseUrl)) {
                    union(left, right)
                }
            }
        }

        val groups = linkedMapOf<Int, MutableList<DiscoveredDevice>>()
        devices.indices.forEach { index ->
            groups.getOrPut(find(index)) { mutableListOf() } += devices[index]
        }

        return groups.values
            .mapNotNull { candidates -> candidates.maxWithOrNull(preference) }
            .sortedBy { it.deviceId.lowercase() }
    }

    private val preference = compareBy<DiscoveredDevice> { it.seenAtMs }
        .thenBy { sourcePriority(it.source) }

    private fun sourcePriority(source: DiscoverySource): Int = when (source) {
        DiscoverySource.MDNS -> 2
        DiscoverySource.UDP -> 1
        DiscoverySource.HTTP -> 0
    }
}
