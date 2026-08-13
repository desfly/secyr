package ua.homeguard.s3.network

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Test

class ExtendedTelemetryParserTest {
    @Test
    fun parsesOutputsTemperaturesElectricalAndAlarms() {
        val json = JSONObject(
            """
            {
              "alarmCount": 2,
              "outputs": [{"index":0,"name":"Valve","state":"on","active":true,"controllable":true}],
              "temperatures": [{"index":0,"name":"Boiler","valueC":63.5,"state":"warning"}],
              "electrical": [{"index":0,"name":"12V bus","voltageV":12.4,"currentA":1.8,"powerW":22.3,"state":"ok"}]
            }
            """.trimIndent()
        )

        val telemetry = ExtendedTelemetryParser.parse(json)

        assertEquals(2, telemetry.alarmCount)
        assertEquals("Valve", telemetry.outputs.single().name)
        assertEquals(true, telemetry.outputs.single().active)
        assertEquals(63.5f, telemetry.temperatures.single().valueC, 0.001f)
        assertEquals(12.4f, telemetry.electrical.single().voltageV ?: 0f, 0.001f)
        assertEquals(1.8f, telemetry.electrical.single().currentA ?: 0f, 0.001f)
        assertEquals(22.3f, telemetry.electrical.single().powerW ?: 0f, 0.001f)
    }

    @Test
    fun missingElectricalValuesStayNull() {
        val telemetry = ExtendedTelemetryParser.parse(
            JSONObject("{\"electrical\":[{\"index\":1,\"name\":\"Battery\",\"state\":\"ok\"}]}")
        )
        val channel = telemetry.electrical.single()
        assertFalse(channel.voltageV != null)
        assertFalse(channel.currentA != null)
        assertFalse(channel.powerW != null)
    }
}
