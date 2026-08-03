package ua.homeguard.app.alarm

data class AlarmAcknowledgementUiState(
    val visible: Boolean = false,
    val alarmSequence: String = "",
    val submitting: Boolean = false,
    val acknowledged: Boolean = false,
    val message: String? = null,
)
