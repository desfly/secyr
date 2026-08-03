package ua.homeguard.s3.ui.infrastructure

import ua.homeguard.s3.model.CompileGateStatus

object CompileGateFormatter {
    fun summary(status: CompileGateStatus): String =
        listOf(
            "preflight=${status.preflightPassed}",
            "source=${status.sourceAuditPassed}",
            "deps=${status.dependencyAuditPassed}",
            "mock=${status.mockSyntaxPassed}",
            "idf=${status.realEspIdfBuildPassed ?: "pending"}",
        ).joinToString(", ")
}
