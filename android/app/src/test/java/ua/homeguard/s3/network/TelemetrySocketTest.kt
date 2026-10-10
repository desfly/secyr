package ua.homeguard.s3.network

import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.flow.MutableStateFlow
import ua.homeguard.s3.model.ControlPath
import ua.homeguard.s3.model.DeviceEndpoint
import ua.homeguard.s3.network.cloud.CloudRuntime
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import okhttp3.mockwebserver.MockResponse
import okhttp3.mockwebserver.MockWebServer
import org.junit.Assert.*
import org.junit.Test
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

class TelemetrySocketTest {
    private fun awaitCondition(condition: () -> Boolean) {
        val deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(3)
        while (!condition() && System.nanoTime() < deadline) Thread.sleep(10)
        assertTrue("Condition did not become true", condition())
    }

    @Test fun openSocketAndInvalidFramesDoNotEstablishTelemetryHealth() {
        val server = MockWebServer()
        val opened = CountDownLatch(1)
        lateinit var peer: WebSocket
        server.enqueue(MockResponse().withWebSocketUpgrade(object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                peer = webSocket
                opened.countDown()
            }
        }))
        server.start()
        val telemetry = TelemetrySocket(1_000)
        try {
            telemetry.connect(server.url("/ws").toString(), "")
            assertTrue(opened.await(3, TimeUnit.SECONDS))
            peer.send("invalid json")
            peer.send("{}")
            peer.send("{\"event\":\"alarm\",\"sequence\":1}")
            Thread.sleep(100)
            assertEquals(TelemetryConnectionState.CONNECTING, telemetry.connection().value)
            assertEquals(0L, telemetry.lastReceivedAtMs().value)
            awaitCondition { telemetry.connection().value == TelemetryConnectionState.OFFLINE }
        } finally {
            telemetry.disconnect()
            server.shutdown()
        }
    }

    @Test fun validSnapshotEstablishesHealthAndSilenceExpiresIt() {
        val server = MockWebServer()
        val opened = CountDownLatch(1)
        lateinit var peer: WebSocket
        server.enqueue(MockResponse().withWebSocketUpgrade(object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                peer = webSocket
                opened.countDown()
            }
        }))
        server.start()
        val telemetry = TelemetrySocket(500)
        try {
            telemetry.connect(server.url("/ws").toString(), "")
            assertTrue(opened.await(3, TimeUnit.SECONDS))
            peer.send("{\"zones\":[0,0,0,0,0,0,0,0]}")
            awaitCondition { telemetry.connection().value == TelemetryConnectionState.CONNECTED }
            val received = telemetry.lastReceivedAtMs().value
            assertTrue(received > 0)
            Thread.sleep(100)
            assertEquals(received, telemetry.lastReceivedAtMs().value)
            awaitCondition { telemetry.connection().value == TelemetryConnectionState.OFFLINE }
            assertEquals(received, telemetry.lastReceivedAtMs().value)
            telemetry.disconnect()
            assertEquals(0L, telemetry.lastReceivedAtMs().value)
            assertEquals(TelemetryConnectionState.IDLE, telemetry.connection().value)
        } finally {
            telemetry.disconnect()
            server.shutdown()
        }
    }
    @Test fun cloudLastSeenOnlyChangesWhenAnotherSnapshotArrives() {
        val server = MockWebServer()
        val opened = CountDownLatch(1)
        lateinit var peer: WebSocket
        server.enqueue(MockResponse().withWebSocketUpgrade(object : WebSocketListener() {
            override fun onOpen(webSocket: WebSocket, response: Response) {
                peer = webSocket
                opened.countDown()
            }
        }))
        server.start()
        val scope = CoroutineScope(SupervisorJob() + Dispatchers.Default)
        val telemetry = TelemetrySocket()
        val endpoint = MutableStateFlow(DeviceEndpoint("HG-TEST", "", server.url("/ws").toString(), ControlPath.CLOUD))
        val cloud = CloudRuntime(scope, endpoint, telemetry)
        try {
            cloud.start()
            telemetry.connect(endpoint.value.websocketUrl, "")
            assertTrue(opened.await(3, TimeUnit.SECONDS))
            peer.send("{\"zones\":[0]}")
            awaitCondition { cloud.state().value == CloudRuntime.State.CONNECTED }
            val first = cloud.lastSeenAtMs().value
            assertTrue(first > 0)
            Thread.sleep(650)
            assertEquals(first, cloud.lastSeenAtMs().value)
            peer.send("{\"zones\":[1]}")
            awaitCondition { cloud.lastSeenAtMs().value > first }
        } finally {
            cloud.stop()
            scope.cancel()
            telemetry.disconnect()
            server.shutdown()
        }
    }
}
