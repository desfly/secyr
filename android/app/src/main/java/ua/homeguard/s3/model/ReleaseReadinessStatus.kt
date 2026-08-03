package ua.homeguard.s3.model

data class ReleaseReadinessStatus(
    val preflight: Boolean,
    val sourceAudit: Boolean,
    val dependencyAudit: Boolean,
    val gpioSafety: Boolean,
    val firmwareBudget: Boolean,
    val mockSyntax: Boolean,
    val mockLink: Boolean,
    val realEspIdfCompile: Boolean?,
)
