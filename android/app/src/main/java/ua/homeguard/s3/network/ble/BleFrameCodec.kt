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
        private data class Key(val type: Int, val messageId: Int)
        private data class Assembly(
            val expectedCount: Int,
            var nextIndex: Int = 0,
            var size: Int = 0,
            val output: ByteArrayOutputStream = ByteArrayOutputStream(),
        )

        private val assemblies = linkedMapOf<Key, Assembly>()

        fun reset() {
            assemblies.clear()
        }

        fun accept(frame: ByteArray): Message? {
            require(frame.size >= HomeGuardBleContract.HEADER_SIZE) { "ble_frame_short" }
            require(frame[0].toInt() and 0xff == HomeGuardBleContract.PROTOCOL_VERSION) { "ble_version" }
            val incomingType = frame[1].toInt() and 0xff
            val incomingId = (frame[2].toInt() and 0xff) or ((frame[3].toInt() and 0xff) shl 8)
            val index = frame[4].toInt() and 0xff
            val count = frame[5].toInt() and 0xff
            require(count > 0 && index < count) { "ble_fragment_index" }

            val key = Key(incomingType, incomingId)
            if (index == 0) {
                // Notifications for telemetry and command/session replies can be
                // interleaved by the ESP. Keep a separate reassembly state per
                // protocol message instead of resetting the one global stream.
                assemblies[key] = Assembly(expectedCount = count)
                while (assemblies.size > MAX_IN_FLIGHT_MESSAGES) {
                    assemblies.remove(assemblies.keys.first())
                }
            }

            val assembly = assemblies[key]
                ?: throw IllegalArgumentException("ble_fragment_sequence")
            if (assembly.expectedCount != count || index != assembly.nextIndex) {
                assemblies.remove(key)
                throw IllegalArgumentException("ble_fragment_sequence")
            }

            val fragmentSize = frame.size - HomeGuardBleContract.HEADER_SIZE
            require(assembly.size + fragmentSize <= HomeGuardBleContract.MAX_MESSAGE_BYTES) {
                assemblies.remove(key)
                "ble_message_too_large"
            }
            assembly.output.write(frame, HomeGuardBleContract.HEADER_SIZE, fragmentSize)
            assembly.size += fragmentSize
            assembly.nextIndex++
            if (assembly.nextIndex != assembly.expectedCount) return null

            assemblies.remove(key)
            return Message(incomingType, incomingId, assembly.output.toByteArray())
        }

        companion object {
            private const val MAX_IN_FLIGHT_MESSAGES = 8
        }
    }
}
