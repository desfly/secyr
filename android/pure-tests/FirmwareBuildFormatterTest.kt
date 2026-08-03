import ua.homeguard.s3.model.FirmwareBuildStatus
import ua.homeguard.s3.ui.infrastructure.FirmwareBuildFormatter

fun main() {
    val pending = FirmwareBuildStatus(
        buildNumber = "0021",
        espIdfVersion = "5.4.2",
        firmwareVersion = "0.21.0",
        gitRevision = "unknown",
        partitionProfile = "production",
        otaCapable = true,
        compiled = false,
    )

    check(
        FirmwareBuildFormatter.summary(pending)
            .contains("not confirmed")
    )
    check(
        FirmwareBuildFormatter.ota(pending)
            .contains("ota_1")
    )

    val built = pending.copy(compiled = true)
    check(
        FirmwareBuildFormatter.summary(built)
            .contains("ESP-IDF 5.4.2")
    )

    println("Firmware build formatter tests PASS")
}
