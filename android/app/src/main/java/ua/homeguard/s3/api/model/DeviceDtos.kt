package ua.homeguard.s3.api.model

data class ZoneDto(
    val id: String,
    val title: String,
    val state: String,
    val alwaysOn: Boolean,
    val alarm: Boolean,
    val millivolts: Double,
)

data class ValveDto(
    val id: String,
    val state: String,
    val emergencyLatched: Boolean,
    val faultCount: Long,
)

data class DeviceStateDto(
    val sequence: String,
    val serverTimeMs: String,
    val securityMode: String,
    val corridorLight: Boolean,
    val siren: Boolean,
    val mainsPresent: Boolean,
    val batteryVoltageV: Double,
    val batteryCurrentA: Double,
    val coldPressureBar: Double,
    val hotPressureBar: Double,
    val coldTemperatureC: Double,
    val hotTemperatureC: Double,
    val zones: List<ZoneDto>,
    val valves: List<ValveDto>,
)

data class CommandRequestDto(
    val requestId: String,
    val actor: String,
    val command: String,
    val target: String = "",
    val value: String = "",
)

data class CommandResponseDto(
    val code: String,
    val message: String,
    val stateSequence: String,
)

data class BuildInfoDto(
    val project: String,
    val build: String,
    val version: String,
    val board: String,
    val module: String,
    val espIdfRequired: String,
    val gitRevision: String,
    val buildTimestampUtc: String,
)
