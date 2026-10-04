package ua.homeguard.s3.network.mqtt

import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.lang.reflect.InvocationTargetException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import org.junit.Assert.*
import org.junit.Test

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
}
