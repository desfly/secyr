package ua.homeguard.s3.network

import org.json.JSONObject
import ua.homeguard.s3.model.*

internal object JsonParsers {
    fun snapshot(json: JSONObject): SystemSnapshot {
        val zones = json.optJSONArray("zones")?.let { array ->
            (0 until array.length()).map { index ->
                val item = array.optJSONObject(index)
                if (item != null) {
                    ZoneStatus(
                        index = item.optInt("index", index),
                        name = item.optString("name", "Zone ${index + 1}"),
                        state = item.optString("state", "unknown"),
                        enabled = item.optBoolean("enabled", true),
                    )
                } else {
                    val code = array.optInt(index, -1)
                    ZoneStatus(
                        index = index,
                        name = "Zone ${index + 1}",
                        state = zoneState(code),
                        enabled = code != 3,
                    )
                }
            }
        }.orEmpty()

        val pressureCodes = json.optJSONArray("pressures")
        val pressureValues = json.optJSONArray("pressure_values")
        val pressureValid = json.optJSONArray("pressure_valid")
        val pressures = when {
            pressureCodes == null -> emptyList()
            pressureCodes.length() > 0 && pressureCodes.optJSONObject(0) != null ->
                (0 until pressureCodes.length()).map { index ->
                    val item = pressureCodes.getJSONObject(index)
                    PressureStatus(
                        index = item.optInt("index", index),
                        value = item.optDouble("value", 0.0).toFloat(),
                        state = item.optString("state", "unknown"),
                        unit = item.optString("unit", ""),
                        valid = item.optBoolean("valid", !item.optString("state", "unknown").equals("sensor_fault", true)),
                    )
                }
            else -> (0 until pressureCodes.length()).map { index ->
                val code = pressureCodes.optInt(index, 0)
                val valid = pressureValid?.optBoolean(index, code != 4) ?: (code != 4)
                PressureStatus(
                    index = index,
                    value = pressureValues?.optDouble(index, 0.0)?.toFloat() ?: 0.0f,
                    state = pressureState(code),
                    unit = "mV",
                    valid = valid,
                )
            }
        }

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
        } ?: run {
            val values = json.optJSONArray("temperatures_c")
            val valid = json.optJSONArray("temperature_valid")
            val count = json.optInt("temperature_count", values?.length() ?: 0)
            if (values == null) emptyList() else (0 until minOf(count, values.length())).map { index ->
                val isValid = valid?.optBoolean(index, true) ?: true
                TemperatureStatus(
                    index = index,
                    name = "Temperature ${index + 1}",
                    celsius = values.optDouble(index, 0.0).toFloat(),
                    state = if (isValid) "normal" else "sensor_fault",
                )
            }
        }

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
        } ?: if (json.has("battery_valid")) {
            listOf(
                PowerChannelStatus(
                    index = 0,
                    name = "Battery",
                    voltage = json.optDouble("battery_voltage_v", 0.0).toFloat(),
                    current = json.optDouble("battery_current_a", 0.0).toFloat(),
                    power = json.optDouble("battery_power_w", 0.0).toFloat(),
                    state = if (json.optBoolean("battery_valid", false)) "normal" else "sensor_fault",
                )
            )
        } else emptyList()

        return SystemSnapshot(
            sequence = json.optLong("sequence", 0),
            uptimeMs = if (json.has("uptimeMs")) json.optLong("uptimeMs", 0) else json.optLong("uptime_ms", 0),
            mode = systemMode(json),
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

    private fun systemMode(json: JSONObject): SystemMode {
        val raw = json.opt("mode")
        return when (raw) {
            is Number -> when (raw.toInt()) {
                1 -> SystemMode.ARMED_HOME
                2 -> SystemMode.ARMED_AWAY
                3 -> SystemMode.ALARM
                4 -> SystemMode.MAINTENANCE
                else -> SystemMode.DISARMED
            }
            else -> enumValue(raw?.toString().orEmpty(), SystemMode.DISARMED)
        }
    }

    private fun zoneState(code: Int): String = when (code) {
        0 -> "normal"
        1 -> "open"
        2 -> "tamper"
        3 -> "disabled"
        else -> "unknown"
    }

    private fun pressureState(code: Int): String = when (code) {
        0 -> "disabled"
        1 -> "normal"
        2 -> "low"
        3 -> "high"
        4 -> "sensor_fault"
        else -> "unknown"
    }

    private inline fun <reified T : Enum<T>> enumValue(raw: String, fallback: T): T {
        val normalized = raw.trim().replace('-', '_').uppercase()
        return enumValues<T>().firstOrNull { it.name == normalized } ?: fallback
    }
}
