package ua.homeguard.s3.network

import org.junit.Assert.assertEquals
import org.junit.Test

class TelemetrySocketConnectFailureTest {
    @Test
    fun invalidUrlDoesNotThrowAndMovesOffline() {
        val telemetry = TelemetrySocket()

        telemetry.connect("not a websocket url", "token")

        assertEquals(TelemetryConnectionState.OFFLINE, telemetry.connection().value)
    }

    @Test
    fun invalidCertificatePinDoesNotThrowAndMovesOffline() {
        val telemetry = TelemetrySocket()

        telemetry.connect("wss://192.0.2.1/api/v1/telemetry", "token", "bad-pin")

        assertEquals(TelemetryConnectionState.OFFLINE, telemetry.connection().value)
    }
}
