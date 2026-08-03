import ua.homeguard.s3.model.CiBuildStatus
import ua.homeguard.s3.model.CiConclusion
import ua.homeguard.s3.ui.infrastructure.CiBuildFormatter

fun main() {
    val status = CiBuildStatus(
        workflow = "ESP-IDF Build-0023",
        conclusion = CiConclusion.FAILURE,
        revision = "abc123",
        runNumber = 7,
        firmwareArtifactAvailable = false,
        diagnosticsArtifactAvailable = true,
    )

    check(CiBuildFormatter.summary(status).contains("FAILURE"))
    check(CiBuildFormatter.artifacts(status) == "diagnostics")

    println("CI build formatter tests PASS")
}
