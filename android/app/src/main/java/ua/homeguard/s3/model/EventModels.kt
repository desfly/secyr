package ua.homeguard.s3.model

data class SystemEventRecord(
    val sequence: Long,
    val timestampMs: Long,
    val event: String,
    val sourceId: Int = 0,
    val value: Int = 0,
)
