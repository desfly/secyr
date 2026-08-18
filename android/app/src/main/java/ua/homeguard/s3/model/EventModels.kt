package ua.homeguard.s3.model

data class SystemEventRecord(
    val sequence: Long,
    val timestampMs: Long,
    val event: String,
    val sourceId: Int = 0,
    val value: Int = 0,
    val controllerId: String = "",
)

internal fun SystemEventRecord.sameEventIdentity(other: SystemEventRecord): Boolean {
    if (sequence != other.sequence) return false
    val left = controllerId.trim()
    val right = other.controllerId.trim()
    if (left.isBlank() || right.isBlank()) return left.isBlank() && right.isBlank()
    return left.equals(right, ignoreCase = true)
}
