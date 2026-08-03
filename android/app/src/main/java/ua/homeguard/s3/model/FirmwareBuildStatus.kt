package ua.homeguard.s3.model

data class FirmwareBuildStatus(
    val buildNumber: String,
    val espIdfVersion: String,
    val firmwareVersion: String,
    val gitRevision: String,
    val partitionProfile: String,
    val otaCapable: Boolean,
    val compiled: Boolean,
)
