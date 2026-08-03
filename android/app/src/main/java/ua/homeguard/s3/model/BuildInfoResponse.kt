package ua.homeguard.s3.model

data class BuildInfoResponse(
    val project: String,
    val build: String,
    val version: String,
    val board: String,
    val module: String,
    val espIdfRequired: String,
    val gitRevision: String,
    val buildTimestampUtc: String,
)
