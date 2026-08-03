package ua.homeguard.s3.ui.infrastructure

import ua.homeguard.s3.model.CiBuildStatus

object CiBuildFormatter {
    fun summary(status: CiBuildStatus): String =
        "${status.workflow} #${status.runNumber}: ${status.conclusion.name}"

    fun artifacts(status: CiBuildStatus): String =
        buildList {
            if (status.firmwareArtifactAvailable) add("firmware")
            if (status.diagnosticsArtifactAvailable) add("diagnostics")
        }.joinToString(", ").ifBlank { "немає артефактів" }
}
