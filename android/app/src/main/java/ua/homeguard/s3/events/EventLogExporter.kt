package ua.homeguard.s3.events

import ua.homeguard.s3.model.SystemEventRecord

object EventLogExporter {
    fun toCsv(events: List<SystemEventRecord>): String = buildString {
        appendLine("sequence,timestamp_ms,event,source_id,value,category")
        events.forEach { item ->
            append(item.sequence).append(',')
            append(item.timestampMs).append(',')
            append(csv(item.event)).append(',')
            append(item.sourceId).append(',')
            append(item.value).append(',')
            append(EventLogFilterEngine.categoryOf(item).name)
            appendLine()
        }
    }

    fun suggestedFileName(): String = "HomeGuard-S3-events.csv"

    private fun csv(value: String): String {
        val escaped = value.replace("\"", "\"\"")
        return if (escaped.any { it == ',' || it == '\n' || it == '\r' || it == '\"' }) {
            "\"$escaped\""
        } else escaped
    }
}
