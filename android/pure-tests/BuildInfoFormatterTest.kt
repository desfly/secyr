import ua.homeguard.s3.model.BuildInfoResponse
import ua.homeguard.s3.ui.infrastructure.BuildInfoFormatter

fun main() {
    val info = BuildInfoResponse(
        project = "HomeGuard-S3",
        build = "0022",
        version = "0.22.0",
        board = "HW-678 V0.0.0",
        module = "ESP32-S3-WROOM-1-N16R8",
        espIdfRequired = "5.4.2",
        gitRevision = "abc123",
        buildTimestampUtc = "2026-08-03T12:00:00Z",
    )

    check(BuildInfoFormatter.title(info).contains("Build-0022"))
    check(BuildInfoFormatter.platform(info).contains("N16R8"))
    check(BuildInfoFormatter.provenance(info).contains("5.4.2"))

    println("Build info formatter tests PASS")
}
