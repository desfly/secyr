package ua.homeguard.s3.network

import org.json.JSONObject
import ua.homeguard.s3.model.ElectricalTelemetry
import ua.homeguard.s3.model.ExtendedTelemetry
import ua.homeguard.s3.model.OutputTelemetry
import ua.homeguard.s3.model.TemperatureTelemetry

internal object ExtendedTelemetryParser {
    fun parse(json: JSONObject): ExtendedTelemetry {
        val outputs = json.optJSONArray("outputs")?.let { array ->
            (0 until array.length()).map { index ->
                val item = array.getJSONObject(index)
                OutputTelemetry(
                    index = item.optInt("index", index),
                    name = item.optString("name", "Output ${index + 1}"),
                    state = item.optString("state", "unknown"),
                    active = item.optBoolean("active", item.optBoolean("on", false)),
                    controllable = item.optBoolean("controllable", false),
                )
            }
        }.orEmpty()

        val temperatures = json.optJSONArray("temperatures")?.let { array ->
            (0 until array.length()).map { index ->
                val item = array.getJSONObject(index)
                TemperatureTelemetry(
                    index = item.optInt("index", index),
                    name = item.optString("name", "Temperature ${index + 1}"),
                    valueC = item.optDouble("valueC", item.optDouble("value", 0.0)).toFloat(),
                    state = item.optString("state", "unknown"),
                )
            }
        }.orEmpty()

        val electrical = json.optJSONArray("electrical")?.let { array ->
            (0 until array.length()).map { index ->
                val item = array.getJSONObject(index)
                ElectricalTelemetry(
                    index = item.optInt("index", index),
                    name = item.optString("name", "Power ${index + 1}"),
                    voltageV = item.floatOrNull("voltageV", "voltage"),
                    currentA = item.floatOrNull("currentA", "current"),
                    powerW = item.floatOrNull("powerW", "power"),
                    state = item.optString("state", "unknown"),
                )
            }
        }.orEmpty()

        return ExtendedTelemetry(
            alarmCount = json.optInt("alarmCount", json.optInt("activeAlarms", 0)),
            outputs = outputs,
            temperatures = temperatures,
            electrical = electrical,
        )
    }

    private fun JSONObject.floatOrNull(primary: String, fallback: String): Float? {
        val key = when {
            has(primary) && !isNull(primary) -> primary
            has(fallback) && !isNull(fallback) -> fallback
            else -> return null
        }
        val value = optDouble(key, Double.NaN)
        return if (value.isNaN()) null else value.toFloat()
    }
}
