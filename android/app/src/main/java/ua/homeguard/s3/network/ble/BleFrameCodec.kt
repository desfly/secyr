package ua.homeguard.s3.network.ble

import java.io.ByteArrayOutputStream

object BleFrameCodec {
    data class Message(val type: Int, val messageId: Int, val payload: ByteArray)

    fun encode(type: Int, messageId: Int, payload: ByteArray, mtu: Int): List<ByteArray> {
        require(type in 1..255)
        require(messageId in 0..0xffff)
        require(payload.size <= HomeGuardBleContract.MAX_MESSAGE_BYTES)
        val valueLimit = (mtu - 3).coerceAtLeast(HomeGuardBleContract.HEADER_SIZE + 1)
        val payloadLimit = valueLimit - HomeGuardBleContract.HEADER_SIZE
        val count = ((payload.size + payloadLimit - 1) / payloadLimit).coerceAtLeast(1)
        require(count <= 255)
        return (0 until count).map { index ->
            val start = index * payloadLimit
            val end = minOf(start + payloadLimit, payload.size)
            ByteArray(HomeGuardBleContract.HEADER_SIZE + (end - start)).also { frame ->
                frame[0] = HomeGuardBleContract.PROTOCOL_VERSION.toByte()
                frame[1] = type.toByte()
                frame[2] = (messageId and 0xff).toByte()
                frame[3] = ((messageId ushr 8) and 0xff).toByte()
                frame[4] = index.toByte()
                frame[5] = count.toByte()
                if (end > start) payload.copyInto(frame, HomeGuardBleContract.HEADER_SIZE, start, end)
            }
        }
    }

    class Decoder {
        private var type = -1
        private var messageId = -1
        private var expectedCount = 0
        private var nextIndex = 0
        private var size = 0
        private var output = ByteArrayOutputStream()

        fun reset() {
            type = -1; messageId = -1; expectedCount = 0; nextIndex = 0; size = 0
            output = ByteArrayOutputStream()
        }

        fun accept(frame: ByteArray): Message? {
            require(frame.size >= HomeGuardBleContract.HEADER_SIZE) { "ble_frame_short" }
            require(frame[0].toInt() and 0xff == HomeGuardBleContract.PROTOCOL_VERSION) { "ble_version" }
            val incomingType = frame[1].toInt() and 0xff
            val incomingId = (frame[2].toInt() and 0xff) or ((frame[3].toInt() and 0xff) shl 8)
            val index = frame[4].toInt() and 0xff
            val count = frame[5].toInt() and 0xff
            require(count > 0 && index < count) { "ble_fragment_index" }

            if (index == 0) {
                reset()
                type = incomingType; messageId = incomingId; expectedCount = count
            }
            require(type == incomingType && messageId == incomingId && expectedCount == count && index == nextIndex) {
                "ble_fragment_sequence"
            }
            val fragmentSize = frame.size - HomeGuardBleContract.HEADER_SIZE
            require(size + fragmentSize <= HomeGuardBleContract.MAX_MESSAGE_BYTES) { "ble_message_too_large" }
            output.write(frame, HomeGuardBleContract.HEADER_SIZE, fragmentSize)
            size += fragmentSize
            nextIndex++
            if (nextIndex != expectedCount) return null

            val completed = Message(type, messageId, output.toByteArray())
            reset()
            return completed
        }
    }
}
