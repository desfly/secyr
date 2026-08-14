package ua.homeguard.s3.network

import org.json.JSONArray
import org.json.JSONObject
import ua.homeguard.s3.model.*

internal object JsonParsers {
    fun snapshot(json: JSONObject): SystemSnapshot {
        val zones = json.optJSONArray("zones")?.let { array ->
            (0 until array.length()).map { index ->
                val item = array.getJSONObject(index)
                ZoneStatus(
                    index = item.optInt("index", index),
                    name = item.optString("name", "Zone ${index + 1}"),
                    state = item.optString("state", "unknown"),
                    enabled = item.optBoolean("enabled", true),
                )
            }
        }.orEmpty()

        val pressures = json.optJSONArray("pressures")?.let { array ->
            (0 until array.length()).map { index ->
                val item = array.getJSONObject(index)
                PressureStatus(
                    index = item.optInt("index", index),
                    value = item.optDouble("bar", item.optDouble("value", 0.0)).toFloat(),
                    state = item.optString("state", "unknown"),
                    name = item.optString("name", "Pressure ${index + 1}"),
                    currentMa = item.optDouble("currentMa", 0.0).toFloat(),
                )
            }
        }.orEmpty()

        return SystemSnapshot(
            sequence = json.optLong("sequence", 0),
            uptimeMs = json.optLong("uptimeMs", 0),
            mode = enumValue(json.optString("mode"), SystemMode.DISARMED),
            transport = enumValue(json.optString("transport"), Transport.NONE),
            health = enumValue(json.optString("health"), HealthState.UNKNOWN),
            zones = zones,
            pressures = pressures,
            inputs = digitalChannels(json.optJSONArray("inputs"), "Input"),
            outputs = digitalChannels(json.optJSONArray("outputs"), "Output"),
            temperatures = temperatures(json.optJSONArray("temperatures")),
            electrical = electrical(json.optJSONArray("electrical")),
        )
    }

    private fun digitalChannels(array: JSONArray?, fallbackName: String): List<DigitalChannelStatus> =
        array?.let {
            (0 until it.length()).map { index ->
                val item = it.getJSONObject(index)
                DigitalChannelStatus(
                    index = item.optInt("index", index),
                    name = item.optString("name", "$fallbackName ${index + 1}"),
                    active = item.optBoolean("active", item.optBoolean("value", false)),
                    state = item.optString("state", if (item.optBoolean("active", false)) "active" else "inactive"),
                )
            }
        }.orEmpty()

    private fun temperatures(array: JSONArray?): List<TemperatureStatus> =
        array?.let {
            (0 until it.length()).map { index ->
                val item = it.getJSONObject(index)
                TemperatureStatus(
                    index = item.optInt("index", index),
                    name = item.optString("name", "Temperature ${index + 1}"),
                    celsius = item.optDouble("celsius", item.optDouble("value", 0.0)).toFloat(),
                    state = item.optString("state", "unknown"),
                )
            }
        }.orEmpty()

    private fun electrical(array: JSONArray?): List<ElectricalStatus> =
        array?.let {
            (0 until it.length()).map { index ->
                val item = it.getJSONObject(index)
                ElectricalStatus(
                    index = item.optInt("index", index),
                    name = item.optString("name", "Electrical ${index + 1}"),
                    voltage = item.optDouble("voltage", 0.0).toFloat(),
                    current = item.optDouble("current", 0.0).toFloat(),
                    power = item.optDouble("power", 0.0).toFloat(),
                    state = item.optString("state", "unknown"),
                    energyWh = item.optLong("energyWh", 0L),
                    frequencyHz = item.optDouble("frequencyHz", 0.0).toFloat(),
                    powerFactor = item.optDouble("powerFactor", 0.0).toFloat(),
                    alarm = item.optBoolean("alarm", false),
                )
            }
        }.orEmpty()

    fun diagnostics(json: JSONObject): Diagnostics {
        val components = json.optJSONArray("components")?.let { array ->
            (0 until array.length()).map { index ->
                val item = array.getJSONObject(index)
                ComponentHealth(
                    id = item.optString("id", index.toString()),
                    title = item.optString("title", item.optString("id", "component")),
                    state = enumValue(item.optString("state"), HealthState.UNKNOWN),
                    changedAtMs = item.optLong("changedAtMs", 0),
                    failures = item.optInt("failures", 0),
                )
            }
        }.orEmpty()
        return Diagnostics(
            overall = enumValue(json.optString("overall"), HealthState.UNKNOWN),
            activeTransport = enumValue(json.optString("activeTransport"), Transport.NONE),
            failedCount = json.optInt("failedCount", 0),
            degradedCount = json.optInt("degradedCount", 0),
            components = components,
            queuedCommands = json.optInt("queuedCommands", 0),
        )
    }

    private inline fun <reified T : Enum<T>> enumValue(raw: String, fallback: T): T {
        val normalized = raw.trim().replace('-', '_').uppercase()
        return enumValues<T>().firstOrNull { it.name == normalized } ?: fallback
    }
}
