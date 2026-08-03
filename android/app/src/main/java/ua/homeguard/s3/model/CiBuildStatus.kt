package ua.homeguard.s3.model

enum class CiConclusion {
    QUEUED,
    IN_PROGRESS,
    SUCCESS,
    FAILURE,
    CANCELLED,
    UNKNOWN,
}

data class CiBuildStatus(
    val workflow: String,
    val conclusion: CiConclusion,
    val revision: String,
    val runNumber: Long,
    val firmwareArtifactAvailable: Boolean,
    val diagnosticsArtifactAvailable: Boolean,
)
