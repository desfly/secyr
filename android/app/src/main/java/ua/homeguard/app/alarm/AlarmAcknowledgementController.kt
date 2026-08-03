package ua.homeguard.app.alarm

import java.util.UUID

class AlarmAcknowledgementController(
    private val api: AlarmAcknowledgementApi,
    private val actorProvider: () -> String,
) {
    suspend fun acknowledge(
        alarmSequence: String,
    ): AlarmAcknowledgeResult {
        require(alarmSequence.isNotBlank()) {
            "alarmSequence must not be blank"
        }

        val request = AlarmAcknowledgeRequest(
            alarmSequence = alarmSequence,
            actor = actorProvider(),
            requestId = UUID.randomUUID().toString(),
        )

        return api.acknowledge(request).toDomain()
    }
}
