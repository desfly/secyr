package ua.homeguard.s3.network

import org.junit.Assert.assertEquals
import org.junit.Test

class EndpointUrlBuilderTest {
    @Test
    fun httpLocalBecomesWsTelemetry() {
        assertEquals(
            "ws://192.168.55.10:8080${RuntimeApiContract.TELEMETRY_PATH}",
            EndpointUrlBuilder.websocketUrl("http://192.168.55.10:8080/"),
        )
    }

    @Test
    fun httpsLocalBecomesWssTelemetry() {
        assertEquals(
            "wss://homeguard.local${RuntimeApiContract.TELEMETRY_PATH}",
            EndpointUrlBuilder.websocketUrl("https://homeguard.local"),
        )
    }

    @Test
    fun cloudDeviceBaseKeepsSingleSlash() {
        assertEquals(
            "https://cloud.example/v1/devices/HG-001",
            EndpointUrlBuilder.cloudDeviceBase("https://cloud.example/", "HG-001"),
        )
    }

    @Test
    fun unsupportedSchemeDoesNotProduceWebsocketUrl() {
        assertEquals("", EndpointUrlBuilder.websocketUrl("ftp://192.168.1.2"))
    }
}
