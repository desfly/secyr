package ua.homeguard.s3.network

import org.json.JSONArray
import org.json.JSONObject
import ua.homeguard.s3.model.*

internal object JsonParsers {
    fun snapshot(json: JSONObject): SystemSnapshot {
        val zones = parseZones(json.optJSONArray("zones"))
        val pressureValues = json.optJSONArray("pressure_values")
        val pressureValid = json.optJSONArray("pressure_valid")
        val pressures = parsePressures(json.optJSONArray("pressures"), pressureValues, pressureValid)

        val temperatures = json.optJSONArray("temperatures")?.let { array ->
            (0 until array.length()).mapNotNull { index ->
                val item = array.optJSONObject(index) ?: return@mapNotNull null
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
                val isValid = valid?.optBoolean(index, false) ?: true
                TemperatureStatus(index, "Temperature ${index + 1}", values.optDouble(index, 0.0).toFloat(), if (isValid) "normal" else "sensor_fault")
            }
        }

        val powerChannels = json.optJSONArray("powerChannels")?.let { array ->
            (0 until array.length()).mapNotNull { index ->
                val item = array.optJSONObject(index) ?: return@mapNotNull null
                val voltage = item.optDouble("voltage", 0.0).toFloat()
                val current = item.optDouble("current", 0.0).toFloat()
                PowerChannelStatus(index = item.optInt("index", index), name = item.optString("name", "Power ${index + 1}"), voltage = voltage, current = current, power = item.optDouble("power", (voltage * current).toDouble()).toFloat(), state = item.optString("state", "unknown"))
            }
        } ?: if (json.has("battery_voltage_v") || json.has("battery_current_a")) {
            listOf(PowerChannelStatus(0, "Battery", json.optDouble("battery_voltage_v", 0.0).toFloat(), json.optDouble("battery_current_a", 0.0).toFloat(), json.optDouble("battery_power_w", 0.0).toFloat(), if (json.optBoolean("battery_valid", false)) "normal" else "sensor_fault"))
        } else emptyList()

        return SystemSnapshot(
            sequence = json.optLong("sequence", 0),
            uptimeMs = json.optLong("uptimeMs", json.optLong("uptime_ms", 0)),
            mode = parseMode(json.opt("mode")),
            transport = enumValue(json.optString("transport"), Transport.NONE),
            health = enumValue(json.optString("health"), HealthState.UNKNOWN),
            zones = zones,
            pressures = pressures,
            temperatures = temperatures,
            powerChannels = powerChannels,
        )
    }

    private fun parseZones(array: JSONArray?): List<ZoneStatus> {
        if (array == null) return emptyList()
        return (0 until array.length()).mapNotNull { index ->
            when (val raw = array.opt(index)) {
                is JSONObject -> ZoneStatus(index = raw.optInt("index", index), name = raw.optString("name", "Zone ${index + 1}"), state = raw.optString("state", "unknown"), enabled = raw.optBoolean("enabled", true))
                is Number -> {
                    val state = when (raw.toInt()) {
                        0 -> "normal"
                        1 -> "open"
                        2 -> "tamper"
                        3 -> "disabled"
                        4 -> "short"
                        else -> "unknown"
                    }
                    ZoneStatus(index, "Zone ${index + 1}", state, state != "disabled")
                }
                else -> null
            }
        }
    }

    private fun parsePressures(states: JSONArray?, values: JSONArray?, valid: JSONArray?): List<PressureStatus> {
        if (states == null) return emptyList()
        return (0 until states.length()).mapNotNull { index ->
            when (val raw = states.opt(index)) {
                is JSONObject -> PressureStatus(index = raw.optInt("index", index), value = raw.optDouble("value", 0.0).toFloat(), state = raw.optString("state", "unknown"), unit = raw.optString("unit", ""), valid = raw.optBoolean("valid", !raw.optString("state", "unknown").equals("sensor_fault", true)))
                is Number -> {
                    val state = when (raw.toInt()) { 0 -> "disabled"; 1 -> "normal"; 2 -> "low"; 3 -> "high"; 4 -> "sensor_fault"; else -> "unknown" }
                    PressureStatus(index, values?.optDouble(index, 0.0)?.toFloat() ?: 0.0f, state, "mV", valid?.optBoolean(index, state != "sensor_fault") ?: (state != "sensor_fault"))
                }
                else -> null
            }
        }
    }

    private fun parseMode(raw: Any?): SystemMode = when (raw) {
        is Number -> when (raw.toInt()) { 0 -> SystemMode.DISARMED; 1 -> SystemMode.ARMED_HOME; 2 -> SystemMode.ARMED_AWAY; 3 -> SystemMode.ALARM; 4 -> SystemMode.MAINTENANCE; else -> SystemMode.DISARMED }
        else -> enumValue(raw?.toString().orEmpty(), SystemMode.DISARMED)
    }

    fun diagnostics(json: JSONObject): Diagnostics {
        val components = json.optJSONArray("components")?.let { array ->
            (0 until array.length()).map { index ->
                val item = array.getJSONObject(index)
                ComponentHealth(id = item.optString("id", index.toString()), title = item.optString("title", item.optString("id", "component")), state = enumValue(item.optString("state"), HealthState.UNKNOWN), changedAtMs = item.optLong("changedAtMs", 0), failures = item.optInt("failures", 0))
            }
        }.orEmpty()
        return Diagnostics(overall = enumValue(json.optString("overall"), HealthState.UNKNOWN), activeTransport = enumValue(json.optString("activeTransport"), Transport.NONE), failedCount = json.optInt("failedCount", 0), degradedCount = json.optInt("degradedCount", 0), components = components, queuedCommands = json.optInt("queuedCommands", 0))
    }

    private inline fun <reified T : Enum<T>> enumValue(raw: String, fallback: T): T {
        val normalized = raw.trim().replace('-', '_').uppercase()
        return enumValues<T>().firstOrNull { it.name == normalized } ?: fallback
    }
}
