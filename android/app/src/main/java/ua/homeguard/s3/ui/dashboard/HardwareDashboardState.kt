package ua.homeguard.s3.ui.dashboard

import ua.homeguard.s3.model.PressureSensorStatus
import ua.homeguard.s3.model.SupervisedZoneStatus
import ua.homeguard.s3.model.TemperatureChannelStatus
import ua.homeguard.s3.model.WaterValveStatus

data class HardwareDashboardState(
    val zones: List<SupervisedZoneStatus> = emptyList(),
    val pressureSensors: List<PressureSensorStatus> = emptyList(),
    val valves: List<WaterValveStatus> = emptyList(),
    val temperatures: List<TemperatureChannelStatus> = emptyList(),
    val loading: Boolean = false,
    val error: String? = null,
)
