package ua.homeguard.s3.events

import ua.homeguard.s3.model.SystemEventRecord

enum class EventLogCategory {
    ALL,
    CRITICAL,
    STATUS,
    ZONES,
    OTHER,
}

data class EventLogFilter(
    val category: EventLogCategory = EventLogCategory.ALL,
    val query: String = "",
    val sourceId: Int? = null,
)

object EventLogFilterEngine {
    private val criticalEvents = setOf("ALARM", "TAMPER", "BATTERY_LOW", "SENSOR_OFFLINE")
    private val statusEvents = setOf("ARMED", "DISARMED")
    private val zoneEvents = setOf("ZONE_OPEN", "ZONE_CLOSED")

    fun categoryOf(event: SystemEventRecord): EventLogCategory {
        return when (event.event.uppercase()) {
            in criticalEvents -> EventLogCategory.CRITICAL
            in statusEvents -> EventLogCategory.STATUS
            in zoneEvents -> EventLogCategory.ZONES
            else -> EventLogCategory.OTHER
        }
    }

    fun apply(events: List<SystemEventRecord>, filter: EventLogFilter): List<SystemEventRecord> {
        val normalizedQuery = filter.query.trim().uppercase()
        return events.filter { event ->
            val categoryMatches = filter.category == EventLogCategory.ALL || categoryOf(event) == filter.category
            val sourceMatches = filter.sourceId == null || event.sourceId == filter.sourceId
            val queryMatches = normalizedQuery.isBlank() ||
                event.event.uppercase().contains(normalizedQuery) ||
                event.sourceId.toString().contains(normalizedQuery) ||
                event.value.toString().contains(normalizedQuery) ||
                event.sequence.toString().contains(normalizedQuery)
            categoryMatches && sourceMatches && queryMatches
        }
    }
}
