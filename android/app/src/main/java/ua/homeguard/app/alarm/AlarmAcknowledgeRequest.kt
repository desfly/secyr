package ua.homeguard.app.alarm

data class AlarmAcknowledgeRequest(
    val alarmSequence: String,
    val actor: String,
    val requestId: String,
)
