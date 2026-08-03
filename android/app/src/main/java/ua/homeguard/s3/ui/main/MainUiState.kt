package ua.homeguard.s3.ui.main

import ua.homeguard.s3.api.model.DeviceStateDto

data class MainUiState(
    val loading: Boolean = true,
    val sendingCommand: Boolean = false,
    val device: DeviceStateDto? = null,
    val message: String? = null,
    val error: String? = null,
)
