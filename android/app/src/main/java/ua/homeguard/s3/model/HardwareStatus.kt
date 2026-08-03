package ua.homeguard.s3.model

enum class SupervisedZoneState {
    NORMAL,
    ALARM,
    SHORT_CIRCUIT,
    OPEN_CIRCUIT,
    UNSTABLE,
    SENSOR_FAULT,
}

data class SupervisedZoneStatus(
    val id: String,
    val title: String,
    val rawMillivolts: Double,
    val filteredMillivolts: Double,
    val state: SupervisedZoneState,
    val transitionCount: Long,
)

enum class PressureSensorState {
    OK,
    OPEN_LOOP,
    UNDERRANGE,
    OVERRANGE,
    ELECTRICAL_FAULT,
}

data class PressureSensorStatus(
    val title: String,
    val pressureBar: Double,
    val filteredBar: Double,
    val currentMilliamp: Double,
    val rateBarPerSecond: Double,
    val state: PressureSensorState,
)

enum class WaterValveState {
    UNKNOWN,
    OPEN,
    CLOSED,
    OPENING,
    CLOSING,
    JAMMED,
    TIMEOUT,
    MANUAL,
    EMERGENCY_CLOSING,
}

data class WaterValveStatus(
    val title: String,
    val state: WaterValveState,
    val faultCount: Long,
    val emergencyLatched: Boolean,
)

data class TemperatureChannelStatus(
    val title: String,
    val valid: Boolean,
    val latestCelsius: Double,
    val averageCelsius: Double,
    val minimumCelsius: Double,
    val maximumCelsius: Double,
    val rateCelsiusPerMinute: Double,
)
