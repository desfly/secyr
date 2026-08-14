package ua.homeguard.s3.model

enum class Transport { NONE, ETHERNET, WIFI_STA, EMERGENCY_AP }
enum class HealthState { UNKNOWN, OK, DEGRADED, FAILED }
enum class SystemMode { DISARMED, ARMED_HOME, ARMED_AWAY, ALARM, MAINTENANCE }

data class ZoneStatus(
    val index: Int,
    val name: String,
    val state: String,
    val enabled: Boolean,
)

data class PressureStatus(
    val index: Int,
    val value: Float,
    val state: String,
)

data class DigitalChannelStatus(
    val index: Int,
    val name: String,
    val active: Boolean,
    val state: String,
)

data class TemperatureStatus(
    val index: Int,
    val name: String,
    val celsius: Float,
    val state: String,
)

data class ElectricalStatus(
    val index: Int,
    val name: String,
    val voltage: Float,
    val current: Float,
    val power: Float,
    val state: String,
)

data class SystemSnapshot(
    val sequence: Long = 0,
    val uptimeMs: Long = 0,
    val mode: SystemMode = SystemMode.DISARMED,
    val transport: Transport = Transport.NONE,
    val health: HealthState = HealthState.UNKNOWN,
    val zones: List<ZoneStatus> = emptyList(),
    val pressures: List<PressureStatus> = emptyList(),
    val inputs: List<DigitalChannelStatus> = emptyList(),
    val outputs: List<DigitalChannelStatus> = emptyList(),
    val temperatures: List<TemperatureStatus> = emptyList(),
    val electrical: List<ElectricalStatus> = emptyList(),
)
