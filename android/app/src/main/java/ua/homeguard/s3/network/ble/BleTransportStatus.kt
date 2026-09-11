package ua.homeguard.s3.network.ble

import ua.homeguard.s3.network.TransportKind
import ua.homeguard.s3.network.TransportStatus

/** Converts BLE runtime state into the common transport matrix model. */
object BleTransportStatus {
    fun from(state: BleHomeGuardClient.State): TransportStatus = when (state) {
        BleHomeGuardClient.State.READY -> TransportStatus(
            kind = TransportKind.BLE,
            available = true,
            authenticated = true,
        )

        BleHomeGuardClient.State.CONNECTED,
        BleHomeGuardClient.State.AUTHENTICATING,
        BleHomeGuardClient.State.PROVISIONING,
        -> TransportStatus(
            kind = TransportKind.BLE,
            available = true,
            authenticated = false,
        )

        BleHomeGuardClient.State.CONNECTING,
        BleHomeGuardClient.State.DISCOVERING,
        BleHomeGuardClient.State.SUBSCRIBING,
        -> TransportStatus(
            kind = TransportKind.BLE,
            available = false,
            authenticated = false,
        )

        BleHomeGuardClient.State.IDLE,
        BleHomeGuardClient.State.OFFLINE,
        BleHomeGuardClient.State.ERROR,
        -> TransportStatus(
            kind = TransportKind.BLE,
            available = false,
            authenticated = false,
        )
    }
}
