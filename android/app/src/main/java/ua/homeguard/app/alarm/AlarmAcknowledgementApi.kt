package ua.homeguard.app.alarm

interface AlarmAcknowledgementApi {
    suspend fun acknowledge(
        request: AlarmAcknowledgeRequest,
    ): AlarmAcknowledgeNetworkResponse
}

data class AlarmAcknowledgeNetworkResponse(
    val result: String,
    val replayed: Boolean,
    val alarmSequence: String,
    val acknowledged: Boolean,
    val acknowledgedAtMs: String,
    val acknowledgedBy: String,
)

fun AlarmAcknowledgeNetworkResponse.toDomain(): AlarmAcknowledgeResult =
    when (result) {
        "accepted" -> AlarmAcknowledgeResult.Accepted
        "already_acknowledged" -> AlarmAcknowledgeResult.AlreadyAcknowledged
        "no_active_alarm" -> AlarmAcknowledgeResult.NoActiveAlarm
        else -> AlarmAcknowledgeResult.Rejected(result)
    }
