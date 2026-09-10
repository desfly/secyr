package ua.homeguard.s3.network

import org.json.JSONObject
import ua.homeguard.s3.model.*

internal object JsonParsers {
    fun liveZones(json: JSONObject): List<ZoneStatus> {
        val array = json.optJSONArray("zones") ?: return emptyList()
        return (0 until array.length()).mapNotNull { index ->
            val item = array.optJSONObject(index) ?: return@mapNotNull null
            val id = item.optInt("id", index + 1).coerceAtLeast(1)
            ZoneStatus(
                index = id - 1,
                name = item.optString("name", "Zone $id"),
                state = item.optString("state", "unknown"),
                enabled = item.optBoolean("valid", false),
            )
        }.sortedBy { it.index }
    }

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
                    val raw = array.optInt(index, -1)
                    ZoneStatus(
                        index = index,
                        name = "Zone ${index + 1}",
                        state = when (raw) {
                            0 -> "normal"
                            1 -> "open"
                            2 -> "tamper"
                            3 -> "disabled"
                            else -> "unknown"
                        },
                        enabled = raw != 3 && raw >= 0,
                    )
                }
            }
        }.orEmpty()

        val pressures = json.optJSONArray("pressures")?.let { array ->
            (0 until array.length()).map { index ->
                val item = array.optJSONObject(index)
                if (item != null) {
                    PressureStatus(
                        index = item.optInt("index", index),
                        value = item.optDouble("value", 0.0).toFloat(),
                        state = item.optString("state", "unknown"),
                        unit = item.optString("unit", ""),
                        valid = item.optBoolean("valid", !item.optString("state", "unknown").equals("sensor_fault", true)),
                    )
                } else {
                    val raw = array.optInt(index, -1)
                    PressureStatus(
                        index = index,
                        value = json.optJSONArray("pressure_values")?.optDouble(index, 0.0)?.toFloat() ?: 0f,
                        state = when (raw) {
                            0 -> "disabled"
                            1 -> "normal"
                            2 -> "low"
                            3 -> "high"
                            4 -> "sensor_fault"
                            else -> "unknown"
                        },
                        valid = json.optJSONArray("pressure_valid")?.optBoolean(index, false) ?: false,
                    )
                }
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
        } ?: run {
            val values = json.optJSONArray("temperatures_c")
            val valid = json.optJSONArray("temperature_valid")
            val count = json.optInt("temperature_count", values?.length() ?: 0).coerceAtLeast(0)
            if (values == null) emptyList() else (0 until minOf(count, values.length())).map { index ->
                val isValid = valid?.optBoolean(index, false) ?: false
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
        } ?: if (json.has("battery_voltage_v") || json.has("battery_current_a") || json.has("battery_power_w")) {
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
        return if (raw is Number) {
            when (raw.toInt()) {
                0 -> SystemMode.DISARMED
                1 -> SystemMode.ARMED_HOME
                2 -> SystemMode.ARMED_AWAY
                3 -> SystemMode.ALARM
                4 -> SystemMode.MAINTENANCE
                else -> SystemMode.DISARMED
            }
        } else {
            enumValue(raw?.toString().orEmpty(), SystemMode.DISARMED)
        }
    }

    private inline fun <reified T : Enum<T>> enumValue(raw: String, fallback: T): T {
        val normalized = raw.trim().replace('-', '_').uppercase()
        return enumValues<T>().firstOrNull { it.name == normalized } ?: fallback
    }
}
