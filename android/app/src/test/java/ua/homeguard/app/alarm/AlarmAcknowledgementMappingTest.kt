package ua.homeguard.app.alarm

object AlarmAcknowledgementMappingTest {
    @JvmStatic
    fun main(args: Array<String>) {
        check(
            AlarmAcknowledgeNetworkResponse(
                result = "accepted",
                replayed = false,
                alarmSequence = "10",
                acknowledged = true,
                acknowledgedAtMs = "1000",
                acknowledgedBy = "android",
            ).toDomain() == AlarmAcknowledgeResult.Accepted
        )

        check(
            AlarmAcknowledgeNetworkResponse(
                result = "already_acknowledged",
                replayed = true,
                alarmSequence = "10",
                acknowledged = true,
                acknowledgedAtMs = "1000",
                acknowledgedBy = "android",
            ).toDomain() == AlarmAcknowledgeResult.AlreadyAcknowledged
        )

        check(
            AlarmAcknowledgeNetworkResponse(
                result = "no_active_alarm",
                replayed = false,
                alarmSequence = "10",
                acknowledged = false,
                acknowledgedAtMs = "0",
                acknowledgedBy = "",
            ).toDomain() == AlarmAcknowledgeResult.NoActiveAlarm
        )

        println("Alarm acknowledgement Android mapping tests PASS")
    }
}
