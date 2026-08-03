package ua.homeguard.s3.model

data class CompileGateStatus(
    val preflightPassed: Boolean,
    val sourceAuditPassed: Boolean,
    val dependencyAuditPassed: Boolean,
    val mockSyntaxPassed: Boolean,
    val realEspIdfBuildPassed: Boolean?,
)
