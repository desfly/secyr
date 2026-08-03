package ua.homeguard.s3.ui.infrastructure

import ua.homeguard.s3.model.ReleaseReadinessStatus

object ReleaseReadinessFormatter {
    fun passedHostGates(status: ReleaseReadinessStatus): Int =
        listOf(
            status.preflight,
            status.sourceAudit,
            status.dependencyAudit,
            status.gpioSafety,
            status.firmwareBudget,
            status.mockSyntax,
            status.mockLink,
        ).count { it }

    fun summary(status: ReleaseReadinessStatus): String =
        "${passedHostGates(status)}/7 host gates, " +
            "ESP-IDF=${status.realEspIdfCompile ?: "pending"}"
}
