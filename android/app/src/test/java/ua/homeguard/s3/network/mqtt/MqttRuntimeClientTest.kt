package ua.homeguard.s3.network.mqtt

import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.lang.reflect.InvocationTargetException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.CoroutineStart
import kotlinx.coroutines.launch
import kotlinx.coroutines.runBlocking
import kotlinx.coroutines.cancelAndJoin
import kotlinx.coroutines.flow.MutableStateFlow
import org.json.JSONObject
import org.junit.Assert.*
import org.junit.Test
import ua.homeguard.s3.model.SystemMode

class MqttRuntimeClientTest {
    private val cfg = MqttRuntimeClient.Config("mqtts://broker.example", deviceId = "HG-TEST")

    private fun client(out: BufferedOutputStream): MqttRuntimeClient =
        MqttRuntimeClient(CoroutineScope(Dispatchers.Unconfined)).also {
            it.javaClass.getDeclaredField("output").apply { isAccessible = true }.set(it, out)
        }

    private fun subscribe(client: MqttRuntimeClient, packets: ByteArray) {
        client.javaClass.getDeclaredMethod("subscribe", BufferedInputStream::class.java,
            MqttRuntimeClient.Config::class.java, String::class.java).apply { isAccessible = true }
            .invoke(client, BufferedInputStream(ByteArrayInputStream(packets)), cfg,
                "homeguard/v1/devices/HG-TEST/responses")
    }

    @Test fun retainedAvailabilityBeforeSubackIsProcessed() {
        val out = BufferedOutputStream(ByteArrayOutputStream())
        val client = client(out)
        val topic = "homeguard/v1/devices/HG-TEST/availability".toByteArray()
        val payload = byteArrayOf(0, topic.size.toByte()) + topic + "online".toByteArray()
        val publish = byteArrayOf(0x31, payload.size.toByte()) + payload
        subscribe(client, publish + byteArrayOf(0x90.toByte(), 3, 0, 2, 1))
        assertEquals("online", client.availability().value)
    }

    @Test fun rejectedSubscriptionDoesNotPassAsConnected() {
        val client = client(BufferedOutputStream(ByteArrayOutputStream()))
        try {
            subscribe(client, byteArrayOf(0x90.toByte(), 3, 0, 2, 0x80.toByte()))
            fail("Rejected SUBACK must fail")
        } catch (error: InvocationTargetException) {
            assertTrue(error.cause is IllegalStateException)
        }
    }

    @Test fun subackMustMatchOutstandingPacket() {
        val client = client(BufferedOutputStream(ByteArrayOutputStream()))
        try {
            subscribe(client, byteArrayOf(0x90.toByte(), 3, 0, 99, 1))
            fail("Mismatched SUBACK must fail")
        } catch (error: InvocationTargetException) {
            assertTrue(error.cause is IllegalStateException)
        }
    }

    @Test fun staleConnectionCannotPingReplacementStream() {
        val bytes = ByteArrayOutputStream()
        val current = BufferedOutputStream(bytes)
        val stale = BufferedOutputStream(ByteArrayOutputStream())
        val client = client(current)
        val ping = client.javaClass.getDeclaredMethod("writePing", BufferedOutputStream::class.java)
            .apply { isAccessible = true }
        ping.invoke(client, stale)
        assertEquals(0, bytes.size())
        ping.invoke(client, current)
        assertArrayEquals(byteArrayOf(0xC0.toByte(), 0), bytes.toByteArray())
    }
    private fun heartbeat(client: MqttRuntimeClient, retained: Boolean) {
        val topic = "homeguard/v1/devices/HG-TEST/heartbeat".toByteArray()
        val payload = byteArrayOf(0, topic.size.toByte()) + topic + "{}".toByteArray()
        client.javaClass.getDeclaredMethod("handlePublish", MqttRuntimeClient.Config::class.java,
            Int::class.javaPrimitiveType, ByteArray::class.java).apply { isAccessible = true }
            .invoke(client, cfg, if (retained) 1 else 0, payload)
    }

    @Test fun retainedHeartbeatCannotEstablishFreshDeviceHealth() {
        val client = client(BufferedOutputStream(ByteArrayOutputStream()))
        heartbeat(client, true)
        assertEquals(0L, client.lastHeartbeatAtMs().value)
        assertEquals("unknown", client.availability().value)
    }

    @Test fun newSessionRequiresAnotherLiveHeartbeat() {
        val client = client(BufferedOutputStream(ByteArrayOutputStream()))
        heartbeat(client, false)
        assertTrue(client.lastHeartbeatAtMs().value > 0)
        client.javaClass.getDeclaredMethod("resetDeviceHealth").apply { isAccessible = true }.invoke(client)
        assertEquals(0L, client.lastHeartbeatAtMs().value)
        assertEquals("unknown", client.availability().value)
        heartbeat(client, true)
        assertEquals(0L, client.lastHeartbeatAtMs().value)
        heartbeat(client, false)
        assertTrue(client.lastHeartbeatAtMs().value > 0)
    }
    private fun publishState(client: MqttRuntimeClient, body: String, retained: Boolean = false) {
        val topic = "homeguard/v1/devices/HG-TEST/state".toByteArray()
        val payload = byteArrayOf(0, topic.size.toByte()) + topic + body.toByteArray()
        client.javaClass.getDeclaredMethod("handlePublish", MqttRuntimeClient.Config::class.java,
            Int::class.javaPrimitiveType, ByteArray::class.java).apply { isAccessible = true }
            .invoke(client, cfg, if (retained) 1 else 0, payload)
    }

    @Test fun liveStateMapsEveryFirmwareArmValue() {
        val client = client(BufferedOutputStream(ByteArrayOutputStream()))
        val modes = listOf(SystemMode.DISARMED, SystemMode.ARMED_HOME, SystemMode.ARMED_AWAY, SystemMode.ALARM)
        modes.forEachIndexed { arm, mode ->
            publishState(client, "{\"seq\":${arm + 1},\"up\":50,\"arm\":$arm}")
            assertEquals(mode, client.deviceState().value?.mode)
            assertEquals(50L, client.deviceState().value?.uptimeSeconds)
            assertTrue(client.deviceState().value!!.receivedAtMs > 0)
        }
    }

    @Test fun retainedMalformedAndDuplicateStateCannotRefreshHealth() {
        val client = client(BufferedOutputStream(ByteArrayOutputStream()))
        publishState(client, "{\"seq\":1,\"up\":50,\"arm\":2}", retained = true)
        assertNull(client.deviceState().value)
        publishState(client, "{\"seq\":2,\"up\":50,\"arm\":2}")
        val previous = client.deviceState().value
        listOf("{}", "invalid", "{\"seq\":3,\"up\":50,\"arm\":9}",
            "{\"seq\":3,\"up\":50,\"arm\":1.5}", "{\"seq\":1,\"up\":50,\"arm\":0}",
            "{\"seq\":2,\"up\":50,\"arm\":0}").forEach { body ->
            publishState(client, body)
            assertEquals(previous, client.deviceState().value)
        }
        client.stop()
        assertNull(client.deviceState().value)
    }

    @Test fun deviceRebootMayRestartStateCounter() {
        val client = client(BufferedOutputStream(ByteArrayOutputStream()))
        publishState(client, "{\"seq\":100,\"up\":500,\"arm\":2}")
        publishState(client, "{\"seq\":1,\"up\":1,\"arm\":0}")
        assertEquals(SystemMode.DISARMED, client.deviceState().value?.mode)
        assertEquals(1L, client.deviceState().value?.sequence)
    }
    @Test fun publishesSignedPacketUnchangedWithoutRetentionAndProtectsPendingRequest() = runBlocking {
        val bytes = ByteArrayOutputStream()
        val client = client(BufferedOutputStream(bytes))
        client.javaClass.getDeclaredField("config").apply { isAccessible = true }.set(client, cfg)
        @Suppress("UNCHECKED_CAST")
        val connection = client.javaClass.getDeclaredField("state").apply { isAccessible = true }
            .get(client) as MutableStateFlow<MqttRuntimeClient.State>
        connection.value = MqttRuntimeClient.State.CONNECTED
        val now = System.currentTimeMillis()
        val json = JSONObject().put("version", 1).put("deviceId", "HG-TEST").put("requestId", "signed-1")
            .put("actor", "user").put("command", "security.arm_away").put("keyEpoch", 1)
            .put("counter", 2).put("issuedAtMs", now).put("expiresAtMs", now + 60_000)
            .put("challenge", "").put("signature", "abcd")
        val envelope = SignedCommandEnvelope.parse(json)
        val sending = launch(start = CoroutineStart.UNDISPATCHED) { client.publishSignedCommand(envelope) }
        try {
            val sent = bytes.toByteArray()
            assertEquals(0x32, sent[0].toInt() and 0xff) // QoS 1, retain=false
            var offset = 1
            while ((sent[offset++].toInt() and 0x80) != 0) { }
            val topicLength = ((sent[offset].toInt() and 0xff) shl 8) or (sent[offset + 1].toInt() and 0xff)
            offset += 2
            assertEquals("homeguard/v1/devices/HG-TEST/commands", String(sent, offset, topicLength, Charsets.UTF_8))
            offset += topicLength + 2 // QoS packet id
            assertEquals(json.toString(), String(sent, offset, sent.size - offset, Charsets.UTF_8))
            try { client.publishSignedCommand(envelope); fail("Duplicate pending id accepted") }
            catch (_: IllegalStateException) { }
            assertEquals(sent.size, bytes.size())
        } finally {
            sending.cancelAndJoin()
            client.stop()
        }
    }
}
