package ua.homeguard.s3.network.ble

import java.util.UUID

object HomeGuardBleContract {
    val SERVICE_UUID: UUID = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
    val RX_UUID: UUID = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
    val TX_UUID: UUID = UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e")

    const val PROTOCOL_VERSION: Int = 1
    const val HEADER_SIZE: Int = 6
    // Real-device trace reaches onMtuChanged(status=133) when requesting 247,
    // then the CCCD subscribe fails and Android terminates the GATT link.
    // Keep the standard ATT MTU for runtime; BleFrameCodec already fragments.
    const val PREFERRED_MTU: Int = 23
    const val MAX_MESSAGE_BYTES: Int = 4096

    object Type {
        const val TELEMETRY = 1
        const val EVENT = 2
        const val COMMAND = 3
        const val COMMAND_REPLY = 4
        const val HELLO_SESSION = 5
        const val REMOTE_EVENT = 6
        const val ERROR = 7

        // Factory commissioning path. These messages are intentionally separate
        // from the authenticated runtime command channel so a fresh device can
        // be provisioned before an Admin user/PIN exists.
        const val PROVISIONING_AUTHORIZE = 8
        const val PROVISIONING_APPLY = 9
        const val PROVISIONING_REPLY = 10
    }
}
