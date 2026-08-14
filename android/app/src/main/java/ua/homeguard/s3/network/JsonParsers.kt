package ua.homeguard.s3.network

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
                    value = item.optDouble("value", 0.0).toFloat(),
                    state = item.optString("state", "unknown"),
                    unit = item.optString("unit", ""),
                    valid = item.optBoolean("valid", !item.optString("state", "unknown").equals("sensor_fault", true)),
                )
            }
        }.orEmpty()

        val temperatures = json.optJSONArray("temperatures")?.let { array ->
            (0 until array.length()).map { index ->
                val item = array.getJSONObject(index)
                TemperatureStatus(
                    index = item.optInt("index", index),
                    name = item.optString("name", "Temperature ${index + 1}"),
                    celsius = item.optDouble("celsius", item.optDouble("value", 0.0)).toFloat(),
                    state = item.optString("state", "unknown"),
                )
            }
        }.orEmpty()

        val powerChannels = json.optJSONArray("powerChannels")?.let { array ->
            (0 until array.length()).map { index ->
                val item = array.getJSONObject(index)
                val voltage = item.optDouble("voltage", 0.0).toFloat()
                val current = item.optDouble("current", 0.0).toFloat()
                PowerChannelStatus(
                    index = item.optInt("index", index),
                    name = item.optString("name", "Power ${index + 1}"),
                    voltage = voltage,
                    current = current,
                    power = item.optDouble("power", (voltage * current).toDouble()).toFloat(),
                    state = item.optString("state", "unknown"),
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
            temperatures = temperatures,
            powerChannels = powerChannels,
        )
    }

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
