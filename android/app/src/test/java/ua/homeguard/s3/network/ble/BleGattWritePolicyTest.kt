package ua.homeguard.s3.network.ble

import android.bluetooth.BluetoothGattCharacteristic
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Test

class BleGattWritePolicyTest {
    @Test
    fun `framed transport prefers write command when RX supports both modes`() {
        val properties = BluetoothGattCharacteristic.PROPERTY_WRITE or
            BluetoothGattCharacteristic.PROPERTY_WRITE_NO_RESPONSE

        assertEquals(
            BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE,
            BleHomeGuardClient.preferredWriteType(properties),
        )
    }

    @Test
    fun `framed transport falls back to write request when command is unavailable`() {
        assertEquals(
            BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT,
            BleHomeGuardClient.preferredWriteType(BluetoothGattCharacteristic.PROPERTY_WRITE),
        )
    }

    @Test
    fun `unwritable characteristic is rejected before GATT write`() {
        assertNull(BleHomeGuardClient.preferredWriteType(BluetoothGattCharacteristic.PROPERTY_READ))
    }
}
