package ua.homeguard.app.alarm

data class AlarmAcknowledgement(
    val alarmSequence: String,
    val acknowledged: Boolean,
    val acknowledgedAtEpochMs: Long?,
    val acknowledgedBy: String?,
)

sealed interface AlarmAcknowledgeResult {
    data object Accepted : AlarmAcknowledgeResult
    data object AlreadyAcknowledged : AlarmAcknowledgeResult
    data object NoActiveAlarm : AlarmAcknowledgeResult
    data class Rejected(val reason: String) : AlarmAcknowledgeResult
}
