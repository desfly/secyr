package ua.homeguard.s3.notifications

import ua.homeguard.s3.model.SystemEventRecord

enum class AlertSeverity { INFO, WARNING, CRITICAL }

data class AlertMessage(
    val severity: AlertSeverity,
    val title: String,
    val text: String,
)

object AlertPolicy {
    fun classify(event: SystemEventRecord): AlertMessage? = when (event.event.uppercase()) {
        "ALARM" -> AlertMessage(AlertSeverity.CRITICAL, "Тривога HomeGuard", "Тривога: джерело ${event.sourceId}")
        "TAMPER" -> AlertMessage(AlertSeverity.CRITICAL, "Саботаж / Tamper", "Tamper: зона або модуль ${event.sourceId}")
        "BATTERY_LOW" -> AlertMessage(AlertSeverity.WARNING, "Низький заряд", "Джерело ${event.sourceId}: низький заряд")
        "SENSOR_OFFLINE" -> AlertMessage(AlertSeverity.WARNING, "Датчик недоступний", "Датчик ${event.sourceId} втратив зв'язок")
        "ARMED" -> AlertMessage(AlertSeverity.INFO, "Система під охороною", "Розділ ${event.sourceId} поставлено під охорону")
        "DISARMED" -> AlertMessage(AlertSeverity.INFO, "Систему знято з охорони", "Розділ ${event.sourceId} знято з охорони")
        "ZONE_OPEN" -> AlertMessage(AlertSeverity.INFO, "Зона відкрита", "Зона ${event.sourceId} відкрита")
        "ZONE_CLOSED" -> AlertMessage(AlertSeverity.INFO, "Зона закрита", "Зона ${event.sourceId} закрита")
        else -> null
    }
}
