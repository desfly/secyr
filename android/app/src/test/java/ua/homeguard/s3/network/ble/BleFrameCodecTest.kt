package ua.homeguard.s3.network.ble

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertThrows
import org.junit.Test

class BleFrameCodecTest {
    @Test
    fun roundTripAcrossMultipleFragments() {
        val payload = ByteArray(900) { (it and 0xff).toByte() }
        val frames = BleFrameCodec.encode(HomeGuardBleContract.Type.TELEMETRY, 0x1234, payload, 247)
        val decoder = BleFrameCodec.Decoder()
        var completed: BleFrameCodec.Message? = null
        frames.forEachIndexed { index, frame ->
            val result = decoder.accept(frame)
            if (index < frames.lastIndex) assertNull(result) else completed = result
        }
        assertEquals(HomeGuardBleContract.Type.TELEMETRY, completed!!.type)
        assertEquals(0x1234, completed!!.messageId)
        assertArrayEquals(payload, completed!!.payload)
    }

    @Test
    fun worksAtMinimumBleMtu() {
        val payload = "{\"hello\":\"homeguard\"}".toByteArray()
        val frames = BleFrameCodec.encode(HomeGuardBleContract.Type.HELLO_SESSION, 7, payload, 23)
        val decoder = BleFrameCodec.Decoder()
        var completed: BleFrameCodec.Message? = null
        frames.forEach { decoder.accept(it)?.let { message -> completed = message } }
        assertArrayEquals(payload, completed!!.payload)
    }

    @Test
    fun rejectsOutOfOrderFragments() {
        val payload = ByteArray(100) { it.toByte() }
        val frames = BleFrameCodec.encode(HomeGuardBleContract.Type.EVENT, 10, payload, 23)
        val decoder = BleFrameCodec.Decoder()
        decoder.accept(frames.first())
        assertThrows(IllegalArgumentException::class.java) { decoder.accept(frames[2]) }
    }

    @Test
    fun rejectsUnknownProtocolVersion() {
        val frame = BleFrameCodec.encode(HomeGuardBleContract.Type.EVENT, 1, byteArrayOf(1), 23).single()
        frame[0] = 2
        assertThrows(IllegalArgumentException::class.java) { BleFrameCodec.Decoder().accept(frame) }
    }
}
