package ua.homeguard.s3.network.ble

import java.util.UUID

object HomeGuardBleContract {
    val SERVICE_UUID: UUID = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
    val RX_UUID: UUID = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
    val TX_UUID: UUID = UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e")

    const val PROTOCOL_VERSION: Int = 1
    const val HEADER_SIZE: Int = 6
    const val PREFERRED_MTU: Int = 247
    const val MAX_MESSAGE_BYTES: Int = 4096

    object Type {
        const val TELEMETRY = 1
        const val EVENT = 2
        const val COMMAND = 3
        const val COMMAND_REPLY = 4
        const val HELLO_SESSION = 5
        const val REMOTE_EVENT = 6
        const val ERROR = 7
    }
}
