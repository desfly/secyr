package ua.homeguard.s3.storage

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import ua.homeguard.s3.model.SystemEventRecord

internal fun mergeEventHistory(
    current: List<SystemEventRecord>,
    event: SystemEventRecord,
    maxEvents: Int = EventHistoryStore.MAX_EVENTS,
): List<SystemEventRecord> {
    if (current.any { it.sequence == event.sequence }) return current
    return (listOf(event) + current)
        .distinctBy { it.sequence }
        .sortedByDescending { it.sequence }
        .take(maxEvents.coerceAtLeast(0))
}

class EventHistoryStore(context: Context) {
    companion object {
        private const val PREFS = "homeguard_event_history"
        private const val KEY_EVENTS = "events"
        const val MAX_EVENTS = 256
    }

    private val preferences = context.applicationContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
    @Volatile private var cached: List<SystemEventRecord> = readPersisted()

    fun load(): List<SystemEventRecord> = cached

    @Synchronized
    fun append(event: SystemEventRecord) {
        val current = cached
        val next = mergeEventHistory(current, event)
        if (next === current) return
        cached = next
        save(next)
    }

    @Synchronized
    fun clear() {
        cached = emptyList()
        preferences.edit().remove(KEY_EVENTS).apply()
    }

    private fun readPersisted(): List<SystemEventRecord> = runCatching {
        val raw = preferences.getString(KEY_EVENTS, "[]").orEmpty()
        val array = JSONArray(raw)
        buildList {
            for (index in 0 until array.length()) {
                val item = array.optJSONObject(index) ?: continue
                add(
                    SystemEventRecord(
                        sequence = item.optLong("sequence", 0),
                        timestampMs = item.optLong("timestampMs", 0),
                        event = item.optString("event", "unknown"),
                        sourceId = item.optInt("sourceId", 0),
                        value = item.optInt("value", 0),
                    )
                )
            }
        }.distinctBy { it.sequence }.sortedByDescending { it.sequence }.take(MAX_EVENTS)
    }.getOrDefault(emptyList())

    private fun save(events: List<SystemEventRecord>) {
        val array = JSONArray()
        events.forEach { event ->
            array.put(
                JSONObject()
                    .put("sequence", event.sequence)
                    .put("timestampMs", event.timestampMs)
                    .put("event", event.event)
                    .put("sourceId", event.sourceId)
                    .put("value", event.value)
            )
        }
        preferences.edit().putString(KEY_EVENTS, array.toString()).apply()
    }
}
