package ua.homeguard.s3.ui.hardware

import ua.homeguard.s3.model.HardwareRuntimeHealth

data class HardwareDiagnosticsState(
    val health: HardwareRuntimeHealth? = null,
    val refreshing: Boolean = false,
    val error: String? = null,
)
