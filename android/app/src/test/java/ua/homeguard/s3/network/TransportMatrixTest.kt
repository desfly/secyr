package ua.homeguard.s3.network

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class TransportMatrixTest {
    @Test
    fun `local transport wins when available and authenticated`() {
        val selection = TransportMatrix.chooseActive(
            listOf(
                ready(TransportKind.LOCAL_HTTP),
                ready(TransportKind.LOCAL_WSS),
                ready(TransportKind.BLE),
                ready(TransportKind.CLOUD_HTTP),
                ready(TransportKind.CLOUD_WSS),
            )
        )

        assertEquals(TransportKind.LOCAL_HTTP, selection.command)
        assertEquals(TransportKind.LOCAL_WSS, selection.telemetry)
        assertTrue(selection.localActive)
        assertFalse(selection.bleActive)
        assertFalse(selection.cloudActive)
    }

    @Test
    fun `ble takes over when local transport is unavailable`() {
        val selection = TransportMatrix.chooseActive(
            listOf(
                unavailable(TransportKind.LOCAL_HTTP),
                unavailable(TransportKind.LOCAL_WSS),
                ready(TransportKind.BLE),
                ready(TransportKind.CLOUD_HTTP),
                ready(TransportKind.CLOUD_WSS),
            )
        )

        assertEquals(TransportKind.BLE, selection.command)
        assertEquals(TransportKind.BLE, selection.telemetry)
        assertTrue(selection.bleActive)
        assertFalse(selection.localActive)
    }

    @Test
    fun `cloud takes over when local and ble are unavailable`() {
        val selection = TransportMatrix.chooseActive(
            listOf(
                unavailable(TransportKind.LOCAL_HTTP),
                unavailable(TransportKind.LOCAL_WSS),
                unavailable(TransportKind.BLE),
                ready(TransportKind.CLOUD_HTTP),
                ready(TransportKind.CLOUD_WSS),
                ready(TransportKind.MQTT),
            )
        )

        assertEquals(TransportKind.CLOUD_HTTP, selection.command)
        assertEquals(TransportKind.CLOUD_WSS, selection.telemetry)
        assertTrue(selection.cloudActive)
        assertFalse(selection.bleActive)
    }

    @Test
    fun `local automatically regains priority after recovery`() {
        val degraded = listOf(
            unavailable(TransportKind.LOCAL_HTTP),
            unavailable(TransportKind.LOCAL_WSS),
            ready(TransportKind.BLE),
            ready(TransportKind.CLOUD_HTTP),
            ready(TransportKind.CLOUD_WSS),
        )
        val recovered = degraded.map {
            when (it.kind) {
                TransportKind.LOCAL_HTTP,
                TransportKind.LOCAL_WSS -> ready(it.kind)
                else -> it
            }
        }

        assertEquals(TransportKind.BLE, TransportMatrix.chooseActive(degraded).command)
        assertEquals(TransportKind.BLE, TransportMatrix.chooseActive(degraded).telemetry)

        val selection = TransportMatrix.chooseActive(recovered)
        assertEquals(TransportKind.LOCAL_HTTP, selection.command)
        assertEquals(TransportKind.LOCAL_WSS, selection.telemetry)
        assertTrue(selection.localActive)
    }

    @Test
    fun `unauthenticated transport is never selected`() {
        val selection = TransportMatrix.chooseActive(
            listOf(
                TransportStatus(TransportKind.LOCAL_HTTP, available = true, authenticated = false),
                TransportStatus(TransportKind.LOCAL_WSS, available = true, authenticated = false),
                ready(TransportKind.BLE),
            )
        )

        assertEquals(TransportKind.BLE, selection.command)
        assertEquals(TransportKind.BLE, selection.telemetry)
    }

    private fun ready(kind: TransportKind) =
        TransportStatus(kind = kind, available = true, authenticated = true)

    private fun unavailable(kind: TransportKind) =
        TransportStatus(kind = kind, available = false, authenticated = false)
}
