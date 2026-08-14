package ua.homeguard.s3.model

import org.junit.Assert.assertEquals
import org.junit.Test

class InputPolarityModelsTest {
    @Test
    fun knownWireValuesMapExactly() {
        assertEquals(InputPolarity.ACTIVE_HIGH, InputPolarity.fromWire("active_high"))
        assertEquals(InputPolarity.ACTIVE_LOW, InputPolarity.fromWire("active_low"))
        assertEquals(InputPolarity.UNKNOWN, InputPolarity.fromWire("unknown"))
    }

    @Test
    fun unexpectedOrMissingWireValuesFailSafeToUnknown() {
        assertEquals(InputPolarity.UNKNOWN, InputPolarity.fromWire(null))
        assertEquals(InputPolarity.UNKNOWN, InputPolarity.fromWire(""))
        assertEquals(InputPolarity.UNKNOWN, InputPolarity.fromWire("active"))
    }

    @Test
    fun outgoingWireValuesMatchFirmwareContract() {
        assertEquals("unknown", InputPolarity.UNKNOWN.wireValue)
        assertEquals("active_high", InputPolarity.ACTIVE_HIGH.wireValue)
        assertEquals("active_low", InputPolarity.ACTIVE_LOW.wireValue)
    }
}
