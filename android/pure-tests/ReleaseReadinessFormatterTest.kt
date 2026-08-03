import ua.homeguard.s3.model.ReleaseReadinessStatus
import ua.homeguard.s3.ui.infrastructure.ReleaseReadinessFormatter

fun main() {
    val status = ReleaseReadinessStatus(
        preflight = true,
        sourceAudit = true,
        dependencyAudit = true,
        gpioSafety = true,
        firmwareBudget = true,
        mockSyntax = true,
        mockLink = true,
        realEspIdfCompile = null,
    )

    check(ReleaseReadinessFormatter.passedHostGates(status) == 7)
    check(ReleaseReadinessFormatter.summary(status).contains("7/7"))
    check(ReleaseReadinessFormatter.summary(status).contains("pending"))

    println("Release readiness formatter tests PASS")
}
