package ua.homeguard.s3.model

data class OutputTelemetry(
    val index: Int,
    val name: String,
    val state: String,
    val active: Boolean,
    val controllable: Boolean = false,
)

data class TemperatureTelemetry(
    val index: Int,
    val name: String,
    val valueC: Float,
    val state: String,
)

data class ElectricalTelemetry(
    val index: Int,
    val name: String,
    val voltageV: Float? = null,
    val currentA: Float? = null,
    val powerW: Float? = null,
    val state: String = "unknown",
)

data class ExtendedTelemetry(
    val alarmCount: Int = 0,
    val outputs: List<OutputTelemetry> = emptyList(),
    val temperatures: List<TemperatureTelemetry> = emptyList(),
    val electrical: List<ElectricalTelemetry> = emptyList(),
)
